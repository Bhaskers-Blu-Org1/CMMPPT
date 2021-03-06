// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.




#include "CoinPragma.hpp"

#include <math.h>

#include "CoinHelperFunctions.hpp"
#include "ClpSimplex.hpp"
#include "ClpFactorization.hpp"
#include "ClpPackedMatrix.hpp"
#include "CoinIndexedVector.hpp"
#include "ClpDualRowDantzig.hpp"
#include "ClpDualRowSteepest.hpp"
#include "ClpPrimalColumnDantzig.hpp"
#include "ClpPrimalColumnSteepest.hpp"
#include "ClpNonLinearCost.hpp"
#include "ClpMessage.hpp"
#include "ClpLinearObjective.hpp"
#include <cfloat>

#include <string>
#include <stdio.h>
#include <iostream>
//#############################################################################

ClpSimplex::ClpSimplex () :

  ClpModel(),
  columnPrimalInfeasibility_(0.0),
  rowPrimalInfeasibility_(0.0),
  columnPrimalSequence_(-2),
  rowPrimalSequence_(-2), 
  columnDualInfeasibility_(0.0),
  rowDualInfeasibility_(0.0),
  columnDualSequence_(-2),
  rowDualSequence_(-2),
  primalToleranceToGetOptimal_(-1.0),
  remainingDualInfeasibility_(0.0),
  largeValue_(1.0e15),
  largestPrimalError_(0.0),
  largestDualError_(0.0),
  largestSolutionError_(0.0),
  dualBound_(1.0e10),
  alpha_(0.0),
  theta_(0.0),
  lowerIn_(0.0),
  valueIn_(0.0),
  upperIn_(0.0),
  dualIn_(0.0),
  lowerOut_(-1),
  valueOut_(-1),
  upperOut_(-1),
  dualOut_(-1),
  dualTolerance_(0.0),
  primalTolerance_(0.0),
  sumDualInfeasibilities_(0.0),
  sumPrimalInfeasibilities_(0.0),
  infeasibilityCost_(1.0e10),
  sumOfRelaxedDualInfeasibilities_(0.0),
  sumOfRelaxedPrimalInfeasibilities_(0.0),
  lower_(NULL),
  rowLowerWork_(NULL),
  columnLowerWork_(NULL),
  upper_(NULL),
  rowUpperWork_(NULL),
  columnUpperWork_(NULL),
  cost_(NULL),
  rowObjectiveWork_(NULL),
  objectiveWork_(NULL),
  sequenceIn_(-1),
  directionIn_(-1),
  sequenceOut_(-1),
  directionOut_(-1),
  pivotRow_(-1),
  lastGoodIteration_(-100),
  dj_(NULL),
  rowReducedCost_(NULL),
  reducedCostWork_(NULL),
  solution_(NULL),
  rowActivityWork_(NULL),
  columnActivityWork_(NULL),
  numberDualInfeasibilities_(0),
  numberDualInfeasibilitiesWithoutFree_(0),
  numberPrimalInfeasibilities_(100),
  numberRefinements_(0),
  pivotVariable_(NULL),
  factorization_(NULL),
  rowScale_(NULL),
  savedSolution_(NULL),
  columnScale_(NULL),
  scalingFlag_(1),
  numberTimesOptimal_(0),
  changeMade_(1),
  algorithm_(0),
  forceFactorization_(-1),
  perturbation_(100),
  nonLinearCost_(NULL),
  specialOptions_(0),
  lastBadIteration_(-999999),
  numberFake_(0),
  progressFlag_(0),
  firstFree_(-1)

{
  int i;
  for (i=0;i<6;i++) {
    rowArray_[i]=NULL;
    columnArray_[i]=NULL;
  }
  saveStatus_=NULL;
  // get an empty factorization so we can set tolerances etc
  factorization_ = new ClpFactorization();
  // Say sparse
  factorization_->sparseThreshold(1);
  // say Steepest pricing
  dualRowPivot_ = new ClpDualRowSteepest();
  // say Steepest pricing
  primalColumnPivot_ = new ClpPrimalColumnSteepest();
  solveType_=1; // say simplex based life form
  
}


//-----------------------------------------------------------------------------

ClpSimplex::~ClpSimplex ()
{
  gutsOfDelete(0);
  delete nonLinearCost_;
}
//#############################################################################
void ClpSimplex::setLargeValue( double value) 
{
  if (value>0.0&&value<COIN_DBL_MAX)
    largeValue_=value;
}
int
ClpSimplex::gutsOfSolution ( const double * rowActivities,
			     const double * columnActivities,
			     double * givenDuals,
			     const double * givenPrimals,
			     bool valuesPass)
{


  // if values pass, save values of basic variables
  double * save = NULL;
  double oldValue=0.0;
  if (valuesPass) {
    assert(algorithm_>0); // only primal at present
    assert(nonLinearCost_);
    int iRow;
    checkPrimalSolution( rowActivities, columnActivities);
    // get correct bounds on all variables
    nonLinearCost_->checkInfeasibilities();
    oldValue = nonLinearCost_->largestInfeasibility();
    save = new double[numberRows_];
    for (iRow=0;iRow<numberRows_;iRow++) {
      int iPivot=pivotVariable_[iRow];
      save[iRow] = solution_[iPivot];
    }
  }
  // do work
  computePrimals(rowActivities, columnActivities);
  // If necessary - override results
  if (givenPrimals) {
    memcpy(columnActivityWork_,givenPrimals,numberColumns_*sizeof(double));
    memset(rowActivityWork_,0,numberRows_*sizeof(double));
    times(-1.0,columnActivityWork_,rowActivityWork_);
  }
  double objectiveModification = 0.0;
  if (algorithm_>0&&nonLinearCost_!=NULL) {
    // primal algorithm
    // get correct bounds on all variables
    nonLinearCost_->checkInfeasibilities();
    objectiveModification += nonLinearCost_->changeInCost();
    if (nonLinearCost_->numberInfeasibilities())
      handler_->message(CLP_SIMPLEX_NONLINEAR,messages_)
	<<nonLinearCost_->changeInCost()
	<<nonLinearCost_->numberInfeasibilities()
	<<CoinMessageEol;
  }
  if (valuesPass) {
#ifdef CLP_DEBUG
    std::cout<<"Largest given infeasibility "<<oldValue
	     <<" now "<<nonLinearCost_->largestInfeasibility()<<std::endl;
#endif
    int numberOut=0;
    if (oldValue<1.0
	&&nonLinearCost_->largestInfeasibility()>1.1*oldValue+1.0e-4||
	largestPrimalError_>1.0e-3) {
      // throw out up to 1000 structurals
      int iRow;
      int * sort = new int[numberRows_];
      // first put back solution and store difference
      for (iRow=0;iRow<numberRows_;iRow++) {
	int iPivot=pivotVariable_[iRow];
	double difference = fabs(solution_[iPivot]=save[iRow]);
	solution_[iPivot]=save[iRow];
	save[iRow]=difference;
      }
      for (iRow=0;iRow<numberRows_;iRow++) {
	int iPivot=pivotVariable_[iRow];

	if (iPivot<numberColumns_) {
	  // column
	  double difference= save[iRow];
	  if (difference>1.0e-4) {
	    sort[numberOut]=iPivot;
	    save[numberOut++]=difference;
	  }
	}
      }
      CoinSort_2(save, save + numberOut, sort,
		 CoinFirstGreater_2<double, int>());
      numberOut = min(1000,numberOut);
      for (iRow=0;iRow<numberOut;iRow++) {
	int iColumn=sort[iRow];
	setColumnStatus(iColumn,superBasic);

      }
      delete [] sort;
    }
    delete [] save;
    if (numberOut)
      return numberOut;
  }

  computeDuals(givenDuals);

  // now check solutions
  checkPrimalSolution( rowActivities, columnActivities);
  objectiveValue_ += objectiveModification;
  checkDualSolution();
  if (handler_->logLevel()>3||(largestPrimalError_>1.0e-2||
			       largestDualError_>1.0e-2)) 
    handler_->message(CLP_SIMPLEX_ACCURACY,messages_)
      <<largestPrimalError_
      <<largestDualError_
      <<CoinMessageEol;
  return 0;
}
void
ClpSimplex::computePrimals ( const double * rowActivities,
				     const double * columnActivities)
{

  //work space
  CoinIndexedVector  * workSpace = rowArray_[0];

  double * array = new double [numberRows_+1]; // +1 for network
  double * save = new double [numberRows_+1];
  double * previous = new double [numberRows_+1];

  // accumulate non basic stuff 
  ClpFillN(array,numberRows_,0.0);

  int iRow;
  // order is this way for scaling
  // Use whole matrix every time to make it easier for ClpMatrixBase
  // So zero out basic
  if (columnActivities!=columnActivityWork_)
    ClpDisjointCopyN(columnActivities,numberColumns_,columnActivityWork_);
  if (rowActivities!=rowActivityWork_)
    ClpDisjointCopyN(rowActivities,numberRows_,rowActivityWork_);
  for (iRow=0;iRow<numberRows_;iRow++) {
    int iPivot=pivotVariable_[iRow];
    solution_[iPivot] = 0.0;
  }
  times(-1.0,columnActivityWork_,array);

  for (iRow=0;iRow<numberRows_;iRow++) {
    array[iRow] += rowActivityWork_[iRow];
  }

  // Ftran adjusted RHS and iterate to improve accuracy
  double lastError=COIN_DBL_MAX;
  int iRefine;
  double * work = workSpace->denseVector();
  factorization_->updateColumn(workSpace,array);

  for (iRefine=0;iRefine<numberRefinements_+1;iRefine++) {
    // save in case we want to restore
    ClpDisjointCopyN ( array, numberRows_ , save);

    // put solution in correct place
    for (iRow=0;iRow<numberRows_;iRow++) {
      int iPivot=pivotVariable_[iRow];
      solution_[iPivot] = array[iRow];
    }
    // check Ax == b  (for all)
    times(-1.0,columnActivityWork_,work);
    for (iRow=0;iRow<numberRows_;iRow++) {
      work[iRow] += rowActivityWork_[iRow];
    }

    largestPrimalError_=0.0;
    for (iRow=0;iRow<numberRows_;iRow++) {
      if (fabs(work[iRow])>largestPrimalError_) {
	largestPrimalError_=fabs(work[iRow]);
      }
      //work[iRow] -= save[iRow];
    }
    if (largestPrimalError_>=lastError) {
      // restore
      double * temp = array;
      array = previous;
      previous=temp;
      break;
    }
    if (iRefine<numberRefinements_&&largestPrimalError_>1.0e-12) {
      // try and make better
      // save this
      double * temp = array;
      array = previous;
      previous=temp;
      double multiplier = 131072.0;
      for (iRow=0;iRow<numberRows_;iRow++) {
	array[iRow] = multiplier*work[iRow];
	work[iRow]=0.0;
      }
      lastError=largestPrimalError_;
      factorization_->updateColumn(workSpace,array);
      multiplier = 1.0/multiplier;
      for (iRow=0;iRow<numberRows_;iRow++) {
	array[iRow] = previous[iRow] + multiplier*array[iRow];
	work[iRow]=0.0;
      }
    }
  }

  // solution as accurate as we are going to get
  ClpFillN(work,numberRows_,0.0);
  // put solution in correct place
  for (iRow=0;iRow<numberRows_;iRow++) {
    int iPivot=pivotVariable_[iRow];
    solution_[iPivot] = array[iRow];
  }
  delete [] previous;
  delete [] array;
  delete [] save;
}
// now dual side
void
ClpSimplex::computeDuals(double * givenDjs)
{
  //work space
  CoinIndexedVector  * workSpace = rowArray_[0];

  double * array = new double [numberRows_+1]; // +1 for network
  double * save = new double [numberRows_+1];
  double * previous = new double [numberRows_+1];

  int iRow;
#ifdef CLP_DEBUG
  workSpace->checkClear();
#endif
  if (!givenDjs) {
    for (iRow=0;iRow<numberRows_;iRow++) {
      int iPivot=pivotVariable_[iRow];
      array[iRow]=cost_[iPivot];
    }
  } else {
    // dual values pass - djs may not be zero
    for (iRow=0;iRow<numberRows_;iRow++) {
      int iPivot=pivotVariable_[iRow];
      // make sure zero if done
      if (!pivoted(iPivot))
	givenDjs[iPivot]=0.0;
      array[iRow]=cost_[iPivot]-givenDjs[iPivot];
    }
  }
  ClpDisjointCopyN ( array, numberRows_ , save);

  // Btran basic costs and get as accurate as possible
  double lastError=COIN_DBL_MAX;
  int iRefine;
  double * work = workSpace->denseVector();
  factorization_->updateColumnTranspose(workSpace,array);
  for (iRefine=0;iRefine<numberRefinements_+1;iRefine++) {
    // check basic reduced costs zero
    largestDualError_=0.0;
    // would be faster to do just for basic but this reduces code
    ClpDisjointCopyN(objectiveWork_,numberColumns_,reducedCostWork_);
    transposeTimes(-1.0,array,reducedCostWork_);
    if (!givenDjs) {
      for (iRow=0;iRow<numberRows_;iRow++) {
	int iPivot=pivotVariable_[iRow];
	if (iPivot>=numberColumns_) {
	  // slack
	  work[iRow] = rowObjectiveWork_[iPivot-numberColumns_]
	    + array[iPivot-numberColumns_];
	} else {
	  // column
	work[iRow] = reducedCostWork_[iPivot];
	}
	if (fabs(work[iRow])>largestDualError_) {
	  largestDualError_=fabs(work[iRow]);
	}
      }
    } else {
      for (iRow=0;iRow<numberRows_;iRow++) {
	int iPivot=pivotVariable_[iRow];
	if (iPivot>=numberColumns_) {
	  // slack
	  work[iRow] = rowObjectiveWork_[iPivot-numberColumns_]
	    + array[iPivot-numberColumns_]-givenDjs[iPivot];
	} else {
	  // column
	  work[iRow] = reducedCostWork_[iPivot]- givenDjs[iPivot];
	}
	if (fabs(work[iRow])>largestDualError_) {
	  largestDualError_=fabs(work[iRow]);
	  //assert (largestDualError_<1.0e-7);
	  //if (largestDualError_>1.0e-7)
	  //printf("large dual error %g\n",largestDualError_);
	}
      }
    }
    if (largestDualError_>=lastError) {
      // restore
      double * temp = array;
      array = previous;
      previous=temp;
      break;
    }
    if (iRefine<numberRefinements_&&largestDualError_>1.0e-20
	&&!givenDjs) {
      // try and make better
      // save this
      double * temp = array;
      array = previous;
      previous=temp;
      double multiplier = 131072.0;
      for (iRow=0;iRow<numberRows_;iRow++) {
	array[iRow] = multiplier*work[iRow];
	work[iRow]=0.0;
      }
      lastError=largestDualError_;
      factorization_->updateColumnTranspose(workSpace,array);
      multiplier = 1.0/multiplier;
      for (iRow=0;iRow<numberRows_;iRow++) {
	array[iRow] = previous[iRow] + multiplier*array[iRow];
	work[iRow]=0.0;
      }
    } else {
      break;
    }
  }
  ClpFillN(work,numberRows_,0.0);
  // now look at dual solution
  for (iRow=0;iRow<numberRows_;iRow++) {
    // slack
    double value = array[iRow];
    dual_[iRow]=value;
    value += rowObjectiveWork_[iRow];
    rowReducedCost_[iRow]=value;
  }
  ClpDisjointCopyN(objectiveWork_,numberColumns_,reducedCostWork_);
  transposeTimes(-1.0,dual_,reducedCostWork_);
  // If necessary - override results
  if (givenDjs) {
    // restore accurate duals
    memcpy(givenDjs,dj_,(numberRows_+numberColumns_)*sizeof(double));
  }

  delete [] previous;
  delete [] array;
  delete [] save;
}
/* Given an existing factorization computes and checks 
   primal and dual solutions.  Uses input arrays for variables at
   bounds.  Returns feasibility states */
int ClpSimplex::getSolution ( const double * rowActivities,
			       const double * columnActivities)
{
  if (!factorization_->status()) {
    // put in standard form
    createRim(7+8+16+32);
    // do work
    gutsOfSolution ( rowActivities, columnActivities,NULL,NULL);
    // release extra memory
    deleteRim(0);
  }
  return factorization_->status();
}
/* Given an existing factorization computes and checks 
   primal and dual solutions.  Uses current problem arrays for
   bounds.  Returns feasibility states */
int ClpSimplex::getSolution ( )
{
  double * rowActivities = new double[numberRows_];
  double * columnActivities = new double[numberColumns_];
  ClpDisjointCopyN ( rowActivityWork_, numberRows_ , rowActivities);
  ClpDisjointCopyN ( columnActivityWork_, numberColumns_ , columnActivities);
  int status = getSolution( rowActivities, columnActivities);
  delete [] rowActivities;
  delete [] columnActivities;
  return status;
}
// Factorizes using current basis.  This is for external use
// Return codes are as from ClpFactorization
int ClpSimplex::factorize ()
{
  // put in standard form
  createRim(7+8+16+32,false);
  // do work
  int status = internalFactorize(-1);
  // release extra memory
  deleteRim(0);

  return status;
}

/* Factorizes using current basis.  
      solveType - 1 iterating, 0 initial, -1 external 
      If 10 added then in primal values pass
*/
/* Return codes are as from ClpFactorization unless initial factorization
   when total number of singularities is returned */
int ClpSimplex::internalFactorize ( int solveType)
{

  int iRow,iColumn;
  int totalSlacks=numberRows_;

  bool valuesPass=false;
  if (solveType>=10) {
    valuesPass=true;
    solveType -= 10;
  }
#ifdef CLP_DEBUG
  if (solveType==1) {
    int numberFreeIn=0,numberFreeOut=0;
    double biggestDj=0.0;
    for (iColumn=0;iColumn<numberColumns_;iColumn++) {
      switch(getColumnStatus(iColumn)) {
	
      case basic:
	if (columnLower_[iColumn]<-largeValue_
	    &&columnUpper_[iColumn]>largeValue_) 
	  numberFreeIn++;
	break;
      default:
	if (columnLower_[iColumn]<-largeValue_
	    &&columnUpper_[iColumn]>largeValue_) {
	  numberFreeOut++;
	  biggestDj = max(fabs(dj_[iColumn]),biggestDj);
	}
	break;
      }
    }
    if (numberFreeIn+numberFreeOut)
      printf("%d in basis, %d out - largest dj %g\n",
	     numberFreeIn,numberFreeOut,biggestDj);
  }
#endif
  if (!solveType) {
    if (!valuesPass) {
      // not values pass so set to bounds
      bool allSlack=true;
      if (status_) {
	for (iRow=0;iRow<numberRows_;iRow++) {
	  if (getRowStatus(iRow)!=basic) {
	    allSlack=false;
	    break;
	  }
	}
      }
      if (!allSlack) {
	// set values from warm start (if sensible)
	int numberBasic=0;
	for (iRow=0;iRow<numberRows_;iRow++) {
	  switch(getRowStatus(iRow)) {
	    
	  case basic:
	    numberBasic++;
	    break;
	  case atUpperBound:
	    rowActivityWork_[iRow]=rowUpperWork_[iRow];
	    if (rowActivityWork_[iRow]>largeValue_) {
	      if (rowLowerWork_[iRow]>-largeValue_) {
		rowActivityWork_[iRow]=rowLowerWork_[iRow];
		setRowStatus(iRow,atLowerBound);
	      } else {
		// say free
		setRowStatus(iRow,isFree);
		rowActivityWork_[iRow]=0.0;
	      }
	    }
	    break;
	  case ClpSimplex::isFixed:
	  case atLowerBound:
	    rowActivityWork_[iRow]=rowLowerWork_[iRow];
	    if (rowActivityWork_[iRow]<-largeValue_) {
	      if (rowUpperWork_[iRow]<largeValue_) {
		rowActivityWork_[iRow]=rowUpperWork_[iRow];
		setRowStatus(iRow,atUpperBound);
	      } else {
		// say free
		setRowStatus(iRow,isFree);
		rowActivityWork_[iRow]=0.0;
	      }
	    }
	    break;
	  case isFree:
	    // may not be free after all
	    if (rowLowerWork_[iRow]<-largeValue_&&rowUpperWork_[iRow]>largeValue_)
	      break;
	    // not really free - fall through to superbasic
	  case superBasic:
	    if (rowUpperWork_[iRow]>largeValue_) {
	      if (rowLowerWork_[iRow]>-largeValue_) {
		rowActivityWork_[iRow]=rowLowerWork_[iRow];
		setRowStatus(iRow,atLowerBound);
	      } else {
		// say free
		setRowStatus(iRow,isFree);
		rowActivityWork_[iRow]=0.0;
	      }
	    } else {
	      if (rowLowerWork_[iRow]>-largeValue_) {
		// set to nearest
		if (fabs(rowActivityWork_[iRow]-rowLowerWork_[iRow])
		    <fabs(rowActivityWork_[iRow]-rowLowerWork_[iRow])) {
		  rowActivityWork_[iRow]=rowLowerWork_[iRow];
		  setRowStatus(iRow,atLowerBound);
		} else {
		  rowActivityWork_[iRow]=rowUpperWork_[iRow];
		  setRowStatus(iRow,atUpperBound);
		}
	      } else {
		rowActivityWork_[iRow]=rowUpperWork_[iRow];
		setRowStatus(iRow,atUpperBound);
	      }
	    }
	    break;
	  }
	}
	totalSlacks=numberBasic;

	for (iColumn=0;iColumn<numberColumns_;iColumn++) {
	  switch(getColumnStatus(iColumn)) {
	    
	  case basic:
	    if (numberBasic==numberRows_) {
	      // take out of basis
	      if (columnLowerWork_[iColumn]>-largeValue_) {
		if (columnActivityWork_[iColumn]-columnLowerWork_[iColumn]<
		    columnUpperWork_[iColumn]-columnActivityWork_[iColumn]) {
		  columnActivityWork_[iColumn]=columnLowerWork_[iColumn];
		  setColumnStatus(iColumn,atLowerBound);
		} else {
		  columnActivityWork_[iColumn]=columnUpperWork_[iColumn];
		  setColumnStatus(iColumn,atUpperBound);
		}
	      } else if (columnUpperWork_[iColumn]<largeValue_) {
		columnActivityWork_[iColumn]=columnUpperWork_[iColumn];
		setColumnStatus(iColumn,atUpperBound);
	      } else {
		columnActivityWork_[iColumn]=0.0;
		setColumnStatus(iColumn,isFree);
	      }
	    } else {
	      numberBasic++;
	    }
	    break;
	  case atUpperBound:
	    columnActivityWork_[iColumn]=columnUpperWork_[iColumn];
	    if (columnActivityWork_[iColumn]>largeValue_) {
	      if (columnLowerWork_[iColumn]<-largeValue_) {
		columnActivityWork_[iColumn]=0.0;
		setColumnStatus(iColumn,isFree);
	      } else {
		columnActivityWork_[iColumn]=columnLowerWork_[iColumn];
		setColumnStatus(iColumn,atLowerBound);
	      }
	    }
	    break;
	  case atLowerBound:
	  case ClpSimplex::isFixed:
	    columnActivityWork_[iColumn]=columnLowerWork_[iColumn];
	    if (columnActivityWork_[iColumn]<-largeValue_) {
	      if (columnUpperWork_[iColumn]>largeValue_) {
		columnActivityWork_[iColumn]=0.0;
		setColumnStatus(iColumn,isFree);
	      } else {
		columnActivityWork_[iColumn]=columnUpperWork_[iColumn];
		setColumnStatus(iColumn,atUpperBound);
	      }
	    }
	    break;
	  case isFree:
	    // may not be free after all
	    if (columnLowerWork_[iColumn]<-largeValue_&&columnUpperWork_[iColumn]>largeValue_)
	      break;
	    // not really free - fall through to superbasic
	  case superBasic:
	    if (columnUpperWork_[iColumn]>largeValue_) {
	      if (columnLowerWork_[iColumn]>-largeValue_) {
		columnActivityWork_[iColumn]=columnLowerWork_[iColumn];
		setColumnStatus(iColumn,atLowerBound);
	      } else {
		// say free
		setColumnStatus(iColumn,isFree);
		columnActivityWork_[iColumn]=0.0;
	      }
	    } else {
	      if (columnLowerWork_[iColumn]>-largeValue_) {
		// set to nearest
		if (fabs(columnActivityWork_[iColumn]-columnLowerWork_[iColumn])
		    <fabs(columnActivityWork_[iColumn]-columnLowerWork_[iColumn])) {
		  columnActivityWork_[iColumn]=columnLowerWork_[iColumn];
		  setColumnStatus(iColumn,atLowerBound);
		} else {
		  columnActivityWork_[iColumn]=columnUpperWork_[iColumn];
		  setColumnStatus(iColumn,atUpperBound);
		}
	      } else {
		columnActivityWork_[iColumn]=columnUpperWork_[iColumn];
		setColumnStatus(iColumn,atUpperBound);
	      }
	    }
	    break;
	  }
	}
      } else {
	// all slack basis
	int numberBasic=0;
	if (!status_) {
	  createStatus();
	}
	for (iRow=0;iRow<numberRows_;iRow++) {
	  double lower=rowLowerWork_[iRow];
	  double upper=rowUpperWork_[iRow];
	  if (lower>-largeValue_||upper<largeValue_) {
	    if (fabs(lower)<=fabs(upper)) {
	      rowActivityWork_[iRow]=lower;
	    } else {
	      rowActivityWork_[iRow]=upper;
	    }
	  } else {
	    rowActivityWork_[iRow]=0.0;
	  }
	  setRowStatus(iRow,basic);
	  numberBasic++;
	}
	for (iColumn=0;iColumn<numberColumns_;iColumn++) {
	  double lower=columnLowerWork_[iColumn];
	  double upper=columnUpperWork_[iColumn];
	  double big_bound = largeValue_;
	  if (lower>-big_bound||upper<big_bound) {
	    if ((getColumnStatus(iColumn)==atLowerBound&&
		 columnActivityWork_[iColumn]==lower)||
		(getColumnStatus(iColumn)==atUpperBound&&
		 columnActivityWork_[iColumn]==upper)) {
	      // status looks plausible
	    } else {
	      // set to sensible
	      if (fabs(lower)<=fabs(upper)) {
		setColumnStatus(iColumn,atLowerBound);
		columnActivityWork_[iColumn]=lower;
	      } else {
		setColumnStatus(iColumn,atUpperBound);
		columnActivityWork_[iColumn]=upper;
	      }
	    }
	  } else {
	    setColumnStatus(iColumn,isFree);
	    columnActivityWork_[iColumn]=0.0;
	  }
	}
      }
    } else {
      // values pass has less coding
      // make row activities correct
      memset(rowActivityWork_,0,numberRows_*sizeof(double));
      times(1.0,columnActivityWork_,rowActivityWork_);
      if (status_) {
	// set values from warm start (if sensible)
	int numberBasic=0;
	for (iRow=0;iRow<numberRows_;iRow++) {
	  if (getRowStatus(iRow)==basic) 
	    numberBasic++;
	  else {
	    setRowStatus(iRow,superBasic);
	    // but put to bound if close
	    if (fabs(rowActivityWork_[iRow]-rowLowerWork_[iRow])
		<=primalTolerance_) {
	      rowActivityWork_[iRow]=rowLowerWork_[iRow];
	      setRowStatus(iRow,atLowerBound);
	    } else if (fabs(rowActivityWork_[iRow]-rowUpperWork_[iRow])
		       <=primalTolerance_) {
	      rowActivityWork_[iRow]=rowUpperWork_[iRow];
	      setRowStatus(iRow,atUpperBound);
	    }
	  }
	  
	}
	totalSlacks=numberBasic;
	for (iColumn=0;iColumn<numberColumns_;iColumn++) {
	  if (getColumnStatus(iColumn)==basic) {
	    if (numberBasic==numberRows_) {
	      // take out of basis
	      setColumnStatus(iColumn,superBasic);
	      // but put to bound if close
	      if (fabs(columnActivityWork_[iColumn]-columnLowerWork_[iColumn])
		  <=primalTolerance_) {
		columnActivityWork_[iColumn]=columnLowerWork_[iColumn];
		setColumnStatus(iColumn,atLowerBound);
	      } else if (fabs(columnActivityWork_[iColumn]
			    -columnUpperWork_[iColumn])
			 <=primalTolerance_) {
		columnActivityWork_[iColumn]=columnUpperWork_[iColumn];
		setColumnStatus(iColumn,atUpperBound);
	      }
	    } else 
	      numberBasic++;
	  } else {
	    setColumnStatus(iColumn,superBasic);
	    // but put to bound if close
	    if (fabs(columnActivityWork_[iColumn]-columnLowerWork_[iColumn])
		<=primalTolerance_) {
	      columnActivityWork_[iColumn]=columnLowerWork_[iColumn];
	      setColumnStatus(iColumn,atLowerBound);
	    } else if (fabs(columnActivityWork_[iColumn]
			  -columnUpperWork_[iColumn])
		       <=primalTolerance_) {
	      columnActivityWork_[iColumn]=columnUpperWork_[iColumn];
	      setColumnStatus(iColumn,atUpperBound);
	    }
	  }
	}
      } else {
	// all slack basis
	int numberBasic=0;
	if (!status_) {
	  createStatus();
	}
	for (iRow=0;iRow<numberRows_;iRow++) {
	  setRowStatus(iRow,basic);
	  numberBasic++;
	}
	for (iColumn=0;iColumn<numberColumns_;iColumn++) {
	  setColumnStatus(iColumn,superBasic);
	  // but put to bound if close
	  if (fabs(columnActivityWork_[iColumn]-columnLowerWork_[iColumn])
	      <=primalTolerance_) {
	    columnActivityWork_[iColumn]=columnLowerWork_[iColumn];
	    setColumnStatus(iColumn,atLowerBound);
	  } else if (fabs(columnActivityWork_[iColumn]
			-columnUpperWork_[iColumn])
		     <=primalTolerance_) {
	    columnActivityWork_[iColumn]=columnUpperWork_[iColumn];
	    setColumnStatus(iColumn,atUpperBound);
	  }
	}
      }
    }
    numberRefinements_=1;
    // set fixed if they are
    for (iRow=0;iRow<numberRows_;iRow++) {
      if (getRowStatus(iRow)!=basic ) {
	if (rowLowerWork_[iRow]==rowUpperWork_[iRow]) {
	  rowActivityWork_[iRow]=rowLowerWork_[iRow];
	  setRowStatus(iRow,isFixed);
	}
      }
    }
    for (iColumn=0;iColumn<numberColumns_;iColumn++) {
      if (getColumnStatus(iColumn)!=basic ) {
	if (columnLowerWork_[iColumn]==columnUpperWork_[iColumn]) {
	  columnActivityWork_[iColumn]=columnLowerWork_[iColumn];
	  setColumnStatus(iColumn,isFixed);
	}
      }
    }
  }
  int status=-99;
  int * rowIsBasic = new int[numberRows_];
  int * columnIsBasic = new int[numberColumns_];
  //returns 0 -okay, -1 singular, -2 too many in basis, -99 memory */
  while (status<-98) {
  
    int i;
    int numberBasic=0;
    for (i=0;i<numberRows_;i++) {
      if (getRowStatus(i) == basic) {
	rowIsBasic[i]=1;
	numberBasic++;
      } else {
	rowIsBasic[i]=-1;
      }
    }
    for (i=0;i<numberColumns_;i++) {
      if (getColumnStatus(i) == basic) {
	columnIsBasic[i]=1;
	numberBasic++;
      } else {
	columnIsBasic[i]=-1;
      }
    }
    assert (numberBasic<=numberRows_);
    while (status==-99) {
      status =  factorization_->factorize(this,matrix_,
					  numberRows_,numberColumns_,
					  rowIsBasic, columnIsBasic,
					  0.0);
      if (status==-99) {
	// get more memory
	factorization_->areaFactor(2.0*factorization_->areaFactor());
      }
    }
    if (!status) {
  /// See whether to redo pivot order
      if (factorization_->needToReorder()||!numberIterations_) {
	// do pivot information
	for (i=0;i<numberRows_;i++) {
	  if (getRowStatus(i) == basic) {
	    pivotVariable_[rowIsBasic[i]]=i+numberColumns_;
	  }
	}
	for (i=0;i<numberColumns_;i++) {
	  if (getColumnStatus(i) == basic) {
	    pivotVariable_[columnIsBasic[i]]=i;
	  }
	}
      }
    } else {
      // leave pivotVariable_ in useful form for cleaning basis
      for (i=0;i<numberRows_;i++) {
	pivotVariable_[i]=-1;
      }
      for (i=0;i<numberRows_;i++) {
	if (getRowStatus(i) == basic) {
	  int iPivot = rowIsBasic[i];
	  if (iPivot>=0) 
	    pivotVariable_[iPivot]=i+numberColumns_;
	}
      }
      for (i=0;i<numberColumns_;i++) {
	if (getColumnStatus(i) == basic) {
	  int iPivot = columnIsBasic[i];
	  if (iPivot>=0) 
	    pivotVariable_[iPivot]=i;
	}
      }
    }
    if (status==-1) {
      if (!solveType) {
	//redo basis - first take ALL columns out
	for (iColumn=0;iColumn<numberColumns_;iColumn++) {
	  if (getColumnStatus(iColumn)==
	      basic) {
	    // take out
	    if (!valuesPass) {
	      double lower=columnLowerWork_[iColumn];
	      double upper=columnUpperWork_[iColumn];
	      double value=columnActivityWork_[iColumn];
	      if (lower>-largeValue_||upper<largeValue_) {
		if (fabs(value-lower)<fabs(value-upper)) {
		  setColumnStatus(iColumn,atLowerBound);
		  columnActivityWork_[iColumn]=lower;
		} else {
		  setColumnStatus(iColumn,atUpperBound);
		  columnActivityWork_[iColumn]=upper;
		}
	      } else {
		setColumnStatus(iColumn,isFree);
	      }
	    } else {
	      setColumnStatus(iColumn,superBasic);
	    }
	  }
	}
	for (iRow=0;iRow<numberRows_;iRow++) {
	  int iSequence=pivotVariable_[iRow];
	  if (iSequence>=0) {
	    // basic
	    if (iSequence>=numberColumns_) {
	      // slack in - leave
	      //assert (iSequence-numberColumns_==iRow);
	    } else {
	      // put back structural
	      setColumnStatus(iSequence,basic);
	    }
	  } else {
	    // put in slack
	    setRowStatus(iRow,basic);
	  }
	}
	// signal repeat
	status=-99;
	// set fixed if they are
	for (iRow=0;iRow<numberRows_;iRow++) {
	  if (getRowStatus(iRow)!=basic ) {
	    if (rowLowerWork_[iRow]==rowUpperWork_[iRow]) {
	      rowActivityWork_[iRow]=rowLowerWork_[iRow];
	      setRowStatus(iRow,isFixed);
	    }
	  }
	}
	for (iColumn=0;iColumn<numberColumns_;iColumn++) {
	  if (getColumnStatus(iColumn)!=basic ) {
	    if (columnLowerWork_[iColumn]==columnUpperWork_[iColumn]) {
	      columnActivityWork_[iColumn]=columnLowerWork_[iColumn];
	      setColumnStatus(iColumn,isFixed);
	    }
	  }
	}
      }
    }
  } 
  delete [] rowIsBasic;
  delete [] columnIsBasic;
  if (status) {
    handler_->message(CLP_SIMPLEX_BADFACTOR,messages_)
      <<status
      <<CoinMessageEol;
    return -1;
  } else if (!solveType) {
    // Initial basis - return number of singularities
    int numberSlacks=0;
    for (iRow=0;iRow<numberRows_;iRow++) {
      if (getRowStatus(iRow) == basic)
	numberSlacks++;
    }
    status= max(numberSlacks-totalSlacks,0);
  }

  // sparse methods
  //if (factorization_->sparseThreshold()) {
    // get default value
    factorization_->sparseThreshold(0);
    factorization_->goSparse();
    //}
  
  return status;
}
/* 
   This does basis housekeeping and does values for in/out variables.
   Can also decide to re-factorize
*/
int 
ClpSimplex::housekeeping(double objectiveChange)
{
  numberIterations_++;
  changeMade_++; // something has happened
  // incoming variable

  handler_->message(CLP_SIMPLEX_HOUSE1,messages_)
    <<directionOut_
    <<directionIn_<<theta_
    <<dualOut_<<dualIn_<<alpha_
    <<CoinMessageEol;
  if (getStatus(sequenceIn_)==isFree) {
    handler_->message(CLP_SIMPLEX_FREEIN,messages_)
      <<sequenceIn_
      <<CoinMessageEol;
  }
  // change of incoming
  char rowcol[]={'R','C'};
  if (pivotRow_>=0)
    pivotVariable_[pivotRow_]=sequenceIn();
  if (upper_[sequenceIn_]>1.0e20&&lower_[sequenceIn_]<-1.0e20)
    progressFlag_ |= 2; // making real progress
  solution_[sequenceIn_]=valueIn_;
  if (upper_[sequenceOut_]-lower_[sequenceOut_]<1.0e-12)
    progressFlag_ |= 1; // making real progress
  if (sequenceIn_!=sequenceOut_) {
    //assert( getStatus(sequenceOut_)== basic);
    setStatus(sequenceIn_,basic);
    if (upper_[sequenceOut_]-lower_[sequenceOut_]>0) {
      if (directionOut_>0) {
	// going to lower
	setStatus(sequenceOut_,atLowerBound);
      } else {
	// going to upper
	setStatus(sequenceOut_,atUpperBound);
      }
    } else {
      // fixed
      setStatus(sequenceOut_,isFixed);
    }
    solution_[sequenceOut_]=valueOut_;
  } else {
    // flip from bound to bound
    if (directionIn_==-1) {
      // as if from upper bound
      setStatus(sequenceIn_, atLowerBound);
    } else {
      // as if from lower bound
      setStatus(sequenceIn_, atUpperBound);
    }
  }
  objectiveValue_ += objectiveChange;
  handler_->message(CLP_SIMPLEX_HOUSE2,messages_)
    <<numberIterations_<<objectiveValue()
    <<rowcol[isColumn(sequenceIn_)]<<sequenceWithin(sequenceIn_)
    <<rowcol[isColumn(sequenceOut_)]<<sequenceWithin(sequenceOut_);
  handler_->printing(algorithm_<0)<<theta_<<dualOut_;
  handler_->printing(algorithm_>0)<<dualIn_<<theta_;
  handler_->message()<<CoinMessageEol;
  if (hitMaximumIterations())
    return 2;
  // only time to re-factorize if one before real time
  // this is so user won't be surprised that maximumPivots has exact meaning
  if (factorization_->pivots()==factorization_->maximumPivots()) {
    return 1;
  } else {
    if (forceFactorization_>0&&
	factorization_->pivots()==forceFactorization_) {
      // relax
      forceFactorization_ = (1+3*forceFactorization_)/2;
      if (forceFactorization_>factorization_->maximumPivots())
	forceFactorization_ = -1; //off
      return 1;
    } else {
      // carry on iterating
      return 0;
    }
  }
}
// Copy constructor. 
ClpSimplex::ClpSimplex(const ClpSimplex &rhs) :
  ClpModel(rhs),
  columnPrimalInfeasibility_(0.0),
  rowPrimalInfeasibility_(0.0),
  columnPrimalSequence_(-2),
  rowPrimalSequence_(-2), 
  columnDualInfeasibility_(0.0),
  rowDualInfeasibility_(0.0),
  columnDualSequence_(-2),
  rowDualSequence_(-2),
  primalToleranceToGetOptimal_(-1.0),
  remainingDualInfeasibility_(0.0),
  largeValue_(1.0e15),
  largestPrimalError_(0.0),
  largestDualError_(0.0),
  largestSolutionError_(0.0),
  dualBound_(1.0e10),
  alpha_(0.0),
  theta_(0.0),
  lowerIn_(0.0),
  valueIn_(0.0),
  upperIn_(0.0),
  dualIn_(0.0),
  lowerOut_(-1),
  valueOut_(-1),
  upperOut_(-1),
  dualOut_(-1),
  dualTolerance_(0.0),
  primalTolerance_(0.0),
  sumDualInfeasibilities_(0.0),
  sumPrimalInfeasibilities_(0.0),
  infeasibilityCost_(1.0e10),
  sumOfRelaxedDualInfeasibilities_(0.0),
  sumOfRelaxedPrimalInfeasibilities_(0.0),
  lower_(NULL),
  rowLowerWork_(NULL),
  columnLowerWork_(NULL),
  upper_(NULL),
  rowUpperWork_(NULL),
  columnUpperWork_(NULL),
  cost_(NULL),
  rowObjectiveWork_(NULL),
  objectiveWork_(NULL),
  sequenceIn_(-1),
  directionIn_(-1),
  sequenceOut_(-1),
  directionOut_(-1),
  pivotRow_(-1),
  lastGoodIteration_(-100),
  dj_(NULL),
  rowReducedCost_(NULL),
  reducedCostWork_(NULL),
  solution_(NULL),
  rowActivityWork_(NULL),
  columnActivityWork_(NULL),
  numberDualInfeasibilities_(0),
  numberDualInfeasibilitiesWithoutFree_(0),
  numberPrimalInfeasibilities_(100),
  numberRefinements_(0),
  pivotVariable_(NULL),
  factorization_(NULL),
  rowScale_(NULL),
  savedSolution_(NULL),
  columnScale_(NULL),
  scalingFlag_(1),
  numberTimesOptimal_(0),
  changeMade_(1),
  algorithm_(0),
  forceFactorization_(-1),
  perturbation_(100),
  nonLinearCost_(NULL),
  specialOptions_(0),
  lastBadIteration_(-999999),
  numberFake_(0),
  progressFlag_(0),
  firstFree_(-1)
{
  int i;
  for (i=0;i<6;i++) {
    rowArray_[i]=NULL;
    columnArray_[i]=NULL;
  }
  saveStatus_=NULL;
  factorization_ = NULL;
  dualRowPivot_ = NULL;
  primalColumnPivot_ = NULL;
  gutsOfDelete(0);
  specialOptions_ =0;
  delete nonLinearCost_;
  nonLinearCost_ = NULL;
  gutsOfCopy(rhs);
  solveType_=1; // say simplex based life form
}
// Copy constructor from model
ClpSimplex::ClpSimplex(const ClpModel &rhs) :
  ClpModel(rhs),
  columnPrimalInfeasibility_(0.0),
  rowPrimalInfeasibility_(0.0),
  columnPrimalSequence_(-2),
  rowPrimalSequence_(-2), 
  columnDualInfeasibility_(0.0),
  rowDualInfeasibility_(0.0),
  columnDualSequence_(-2),
  rowDualSequence_(-2),
  primalToleranceToGetOptimal_(-1.0),
  remainingDualInfeasibility_(0.0),
  largeValue_(1.0e15),
  largestPrimalError_(0.0),
  largestDualError_(0.0),
  largestSolutionError_(0.0),
  dualBound_(1.0e10),
  alpha_(0.0),
  theta_(0.0),
  lowerIn_(0.0),
  valueIn_(0.0),
  upperIn_(0.0),
  dualIn_(0.0),
  lowerOut_(-1),
  valueOut_(-1),
  upperOut_(-1),
  dualOut_(-1),
  dualTolerance_(0.0),
  primalTolerance_(0.0),
  sumDualInfeasibilities_(0.0),
  sumPrimalInfeasibilities_(0.0),
  infeasibilityCost_(1.0e10),
  sumOfRelaxedDualInfeasibilities_(0.0),
  sumOfRelaxedPrimalInfeasibilities_(0.0),
  lower_(NULL),
  rowLowerWork_(NULL),
  columnLowerWork_(NULL),
  upper_(NULL),
  rowUpperWork_(NULL),
  columnUpperWork_(NULL),
  cost_(NULL),
  rowObjectiveWork_(NULL),
  objectiveWork_(NULL),
  sequenceIn_(-1),
  directionIn_(-1),
  sequenceOut_(-1),
  directionOut_(-1),
  pivotRow_(-1),
  lastGoodIteration_(-100),
  dj_(NULL),
  rowReducedCost_(NULL),
  reducedCostWork_(NULL),
  solution_(NULL),
  rowActivityWork_(NULL),
  columnActivityWork_(NULL),
  numberDualInfeasibilities_(0),
  numberDualInfeasibilitiesWithoutFree_(0),
  numberPrimalInfeasibilities_(100),
  numberRefinements_(0),
  pivotVariable_(NULL),
  factorization_(NULL),
  rowScale_(NULL),
  savedSolution_(NULL),
  columnScale_(NULL),
  scalingFlag_(1),
  numberTimesOptimal_(0),
  changeMade_(1),
  algorithm_(0),
  forceFactorization_(-1),
  perturbation_(100),
  nonLinearCost_(NULL),
  specialOptions_(0),
  lastBadIteration_(-999999),
  numberFake_(0),
  progressFlag_(0),
  firstFree_(-1)
{
  int i;
  for (i=0;i<6;i++) {
    rowArray_[i]=NULL;
    columnArray_[i]=NULL;
  }
  saveStatus_=NULL;
  // get an empty factorization so we can set tolerances etc
  factorization_ = new ClpFactorization();
  // say Steepest pricing
  dualRowPivot_ = new ClpDualRowSteepest();
  // say Dantzig pricing
  primalColumnPivot_ = new ClpPrimalColumnDantzig();
  solveType_=1; // say simplex based life form
  
}
// Assignment operator. This copies the data
ClpSimplex & 
ClpSimplex::operator=(const ClpSimplex & rhs)
{
  if (this != &rhs) {
    gutsOfDelete(0);
    specialOptions_=0;
    delete nonLinearCost_;
    nonLinearCost_ = NULL;
    ClpModel::operator=(rhs);
    gutsOfCopy(rhs);
  }
  return *this;
}
void 
ClpSimplex::gutsOfCopy(const ClpSimplex & rhs)
{
  lower_ = ClpCopyOfArray(rhs.lower_,numberColumns_+numberRows_);
  rowLowerWork_ = lower_+numberColumns_;
  columnLowerWork_ = lower_;
  upper_ = ClpCopyOfArray(rhs.upper_,numberColumns_+numberRows_);
  rowUpperWork_ = upper_+numberColumns_;
  columnUpperWork_ = upper_;
  cost_ = ClpCopyOfArray(rhs.cost_,(numberColumns_+numberRows_));
  objectiveWork_ = cost_;
  rowObjectiveWork_ = cost_+numberColumns_;
  dj_ = ClpCopyOfArray(rhs.dj_,numberRows_+numberColumns_);
  if (dj_) {
    reducedCostWork_ = dj_;
    rowReducedCost_ = dj_+numberColumns_;
  }
  solution_ = ClpCopyOfArray(rhs.solution_,numberRows_+numberColumns_);
  if (solution_) {
    columnActivityWork_ = solution_;
    rowActivityWork_ = solution_+numberColumns_;
  }
  if (rhs.pivotVariable_) {
    pivotVariable_ = new int[numberRows_];
    ClpDisjointCopyN ( rhs.pivotVariable_, numberRows_ , pivotVariable_);
  } else {
    pivotVariable_=NULL;
  }
  if (rhs.factorization_) {
    factorization_ = new ClpFactorization(*rhs.factorization_);
  } else {
    factorization_=NULL;
  }
  rowScale_ = ClpCopyOfArray(rhs.rowScale_,numberRows_);
  savedSolution_ = ClpCopyOfArray(rhs.savedSolution_,numberColumns_+numberRows_);
  columnScale_ = ClpCopyOfArray(rhs.columnScale_,numberColumns_);
  int i;
  for (i=0;i<6;i++) {
    rowArray_[i]=NULL;
    if (rhs.rowArray_[i]) 
      rowArray_[i] = new CoinIndexedVector(*rhs.rowArray_[i]);
    columnArray_[i]=NULL;
    if (rhs.columnArray_[i]) 
      columnArray_[i] = new CoinIndexedVector(*rhs.columnArray_[i]);
  }
  if (rhs.saveStatus_) {
    saveStatus_ = ClpCopyOfArray( rhs.saveStatus_,numberColumns_+numberRows_);
  }
  columnPrimalInfeasibility_ = rhs.columnPrimalInfeasibility_;
  columnPrimalSequence_ = rhs.columnPrimalSequence_;
  rowPrimalInfeasibility_ = rhs.rowPrimalInfeasibility_;
  rowPrimalSequence_ = rhs.rowPrimalSequence_;
  columnDualInfeasibility_ = rhs.columnDualInfeasibility_;
  columnDualSequence_ = rhs.columnDualSequence_;
  rowDualInfeasibility_ = rhs.rowDualInfeasibility_;
  rowDualSequence_ = rhs.rowDualSequence_;
  primalToleranceToGetOptimal_ = rhs.primalToleranceToGetOptimal_;
  remainingDualInfeasibility_ = rhs.remainingDualInfeasibility_;
  largeValue_ = rhs.largeValue_;
  largestPrimalError_ = rhs.largestPrimalError_;
  largestDualError_ = rhs.largestDualError_;
  largestSolutionError_ = rhs.largestSolutionError_;
  dualBound_ = rhs.dualBound_;
  alpha_ = rhs.alpha_;
  theta_ = rhs.theta_;
  lowerIn_ = rhs.lowerIn_;
  valueIn_ = rhs.valueIn_;
  upperIn_ = rhs.upperIn_;
  dualIn_ = rhs.dualIn_;
  sequenceIn_ = rhs.sequenceIn_;
  directionIn_ = rhs.directionIn_;
  lowerOut_ = rhs.lowerOut_;
  valueOut_ = rhs.valueOut_;
  upperOut_ = rhs.upperOut_;
  dualOut_ = rhs.dualOut_;
  sequenceOut_ = rhs.sequenceOut_;
  directionOut_ = rhs.directionOut_;
  pivotRow_ = rhs.pivotRow_;
  lastGoodIteration_ = rhs.lastGoodIteration_;
  numberRefinements_ = rhs.numberRefinements_;
  dualTolerance_ = rhs.dualTolerance_;
  primalTolerance_ = rhs.primalTolerance_;
  sumDualInfeasibilities_ = rhs.sumDualInfeasibilities_;
  numberDualInfeasibilities_ = rhs.numberDualInfeasibilities_;
  numberDualInfeasibilitiesWithoutFree_ = 
    rhs.numberDualInfeasibilitiesWithoutFree_;
  sumPrimalInfeasibilities_ = rhs.sumPrimalInfeasibilities_;
  numberPrimalInfeasibilities_ = rhs.numberPrimalInfeasibilities_;
  dualRowPivot_ = rhs.dualRowPivot_->clone(true);
  primalColumnPivot_ = rhs.primalColumnPivot_->clone(true);
  scalingFlag_ = rhs.scalingFlag_;
  numberTimesOptimal_ = rhs.numberTimesOptimal_;
  changeMade_ = rhs.changeMade_;
  algorithm_ = rhs.algorithm_;
  forceFactorization_ = rhs.forceFactorization_;
  perturbation_ = rhs.perturbation_;
  infeasibilityCost_ = rhs.infeasibilityCost_;
  specialOptions_ = rhs.specialOptions_;
  lastBadIteration_ = rhs.lastBadIteration_;
  numberFake_ = rhs.numberFake_;
  progressFlag_ = rhs.progressFlag_;
  firstFree_ = rhs.firstFree_;
  sumOfRelaxedDualInfeasibilities_ = rhs.sumOfRelaxedDualInfeasibilities_;
  sumOfRelaxedPrimalInfeasibilities_ = rhs.sumOfRelaxedPrimalInfeasibilities_;
  if (rhs.nonLinearCost_!=NULL)
    nonLinearCost_ = new ClpNonLinearCost(*rhs.nonLinearCost_);
  else
    nonLinearCost_=NULL;
  solveType_=rhs.solveType_;
}
// type == 0 do everything, most + pivot data, 2 factorization data as well
void 
ClpSimplex::gutsOfDelete(int type)
{
  delete [] lower_;
  lower_=NULL;
  rowLowerWork_=NULL;
  columnLowerWork_=NULL;
  delete [] upper_;
  upper_=NULL;
  rowUpperWork_=NULL;
  columnUpperWork_=NULL;
  delete [] cost_;
  cost_=NULL;
  objectiveWork_=NULL;
  rowObjectiveWork_=NULL;
  delete [] dj_;
  dj_=NULL;
  reducedCostWork_=NULL;
  rowReducedCost_=NULL;
  delete [] solution_;
  solution_=NULL;
  rowActivityWork_=NULL;
  columnActivityWork_=NULL;
  delete [] rowScale_;
  rowScale_ = NULL;
  delete [] savedSolution_;
  savedSolution_ = NULL;
  delete [] columnScale_;
  columnScale_ = NULL;
  if ((specialOptions_&2)==0) {
    delete nonLinearCost_;
    nonLinearCost_ = NULL;
  }
  int i;
  for (i=0;i<6;i++) {
    delete rowArray_[i];
    rowArray_[i]=NULL;
    delete columnArray_[i];
    columnArray_[i]=NULL;
  }
  delete rowCopy_;
  rowCopy_=NULL;
  delete [] saveStatus_;
  saveStatus_=NULL;
  if (!type) {
    // delete everything
    delete factorization_;
    factorization_ = NULL;
    delete [] pivotVariable_;
    pivotVariable_=NULL;
    delete dualRowPivot_;
    dualRowPivot_ = NULL;
    delete primalColumnPivot_;
    primalColumnPivot_ = NULL;
  } else {
    // delete any size information in methods
    if (type>1) {
      factorization_->clearArrays();
      delete [] pivotVariable_;
      pivotVariable_=NULL;
    }
    dualRowPivot_->clearArrays();
    primalColumnPivot_->clearArrays();
  }
}
// This sets largest infeasibility and most infeasible
void 
ClpSimplex::checkPrimalSolution(const double * rowActivities,
					const double * columnActivities)
{
  double * solution;
  int iRow,iColumn;

  objectiveValue_ = 0.0;
  firstFree_ = -1;
  // now look at primal solution
  columnPrimalInfeasibility_=0.0;
  columnPrimalSequence_=-1;
  rowPrimalInfeasibility_=0.0;
  rowPrimalSequence_=-1;
  largestSolutionError_=0.0;
  solution = rowActivityWork_;
  sumPrimalInfeasibilities_=0.0;
  numberPrimalInfeasibilities_=0;
  double primalTolerance = primalTolerance_;
  double relaxedTolerance=primalTolerance_;
  // we can't really trust infeasibilities if there is primal error
  double error = min(1.0e-3,largestPrimalError_);
  // allow tolerance at least slightly bigger than standard
  relaxedTolerance = relaxedTolerance +  error;
  sumOfRelaxedPrimalInfeasibilities_ = 0.0;

  for (iRow=0;iRow<numberRows_;iRow++) {
    //assert (fabs(solution[iRow])<1.0e15||getRowStatus(iRow) == basic);
    double infeasibility=0.0;
    objectiveValue_ += solution[iRow]*rowObjectiveWork_[iRow];
    if (solution[iRow]>rowUpperWork_[iRow]) {
      infeasibility=solution[iRow]-rowUpperWork_[iRow];
    } else if (solution[iRow]<rowLowerWork_[iRow]) {
      infeasibility=rowLowerWork_[iRow]-solution[iRow];
    }
    if (infeasibility>primalTolerance) {
      sumPrimalInfeasibilities_ += infeasibility-primalTolerance_;
      if (infeasibility>relaxedTolerance) 
	sumOfRelaxedPrimalInfeasibilities_ += infeasibility-relaxedTolerance;
      numberPrimalInfeasibilities_ ++;
    }
    if (infeasibility>rowPrimalInfeasibility_) {
      rowPrimalInfeasibility_=infeasibility;
      rowPrimalSequence_=iRow;
    }
    infeasibility = fabs(rowActivities[iRow]-solution[iRow]);
    if (infeasibility>largestSolutionError_)
      largestSolutionError_=infeasibility;
  }
  solution = columnActivityWork_;
  for (iColumn=0;iColumn<numberColumns_;iColumn++) {
    //assert (fabs(solution[iColumn])<1.0e15||getColumnStatus(iColumn) == basic);
    double infeasibility=0.0;
    objectiveValue_ += objectiveWork_[iColumn]*solution[iColumn];
    if (solution[iColumn]>columnUpperWork_[iColumn]) {
      infeasibility=solution[iColumn]-columnUpperWork_[iColumn];
    } else if (solution[iColumn]<columnLowerWork_[iColumn]) {
      infeasibility=columnLowerWork_[iColumn]-solution[iColumn];
    }
    if (infeasibility>columnPrimalInfeasibility_) {
      columnPrimalInfeasibility_=infeasibility;
      columnPrimalSequence_=iColumn;
    }
    if (infeasibility>primalTolerance) {
      sumPrimalInfeasibilities_ += infeasibility-primalTolerance_;
      if (infeasibility>relaxedTolerance) 
	sumOfRelaxedPrimalInfeasibilities_ += infeasibility-relaxedTolerance;
      numberPrimalInfeasibilities_ ++;
    }
    infeasibility = fabs(columnActivities[iColumn]-solution[iColumn]);
    if (infeasibility>largestSolutionError_)
      largestSolutionError_=infeasibility;
  }
}
void 
ClpSimplex::checkDualSolution()
{

  int iRow,iColumn;
  sumDualInfeasibilities_=0.0;
  numberDualInfeasibilities_=0;
  numberDualInfeasibilitiesWithoutFree_=0;
  columnDualInfeasibility_=0.0;
  columnDualSequence_=-1;
  rowDualInfeasibility_=0.0;
  rowDualSequence_=-1;
  int firstFree = -1;
  primalToleranceToGetOptimal_=max(rowPrimalInfeasibility_,
				   columnPrimalInfeasibility_);
  remainingDualInfeasibility_=0.0;
  double relaxedTolerance=dualTolerance_;
  // we can't really trust infeasibilities if there is dual error
  double error = min(1.0e-3,largestDualError_);
  // allow tolerance at least slightly bigger than standard
  relaxedTolerance = relaxedTolerance +  error;
  sumOfRelaxedDualInfeasibilities_ = 0.0;

  for (iColumn=0;iColumn<numberColumns_;iColumn++) {
    if (getColumnStatus(iColumn) != basic) {
      // not basic
      double distanceUp = columnUpperWork_[iColumn]-
	columnActivityWork_[iColumn];
      double distanceDown = columnActivityWork_[iColumn] -
	columnLowerWork_[iColumn];
      if (distanceUp>primalTolerance_) {
	double value = reducedCostWork_[iColumn];
	if (!flagged(iColumn)) {
	  // Check if "free"
	  if (firstFree<0&&distanceDown>primalTolerance_) {
	    if (algorithm_>0) {
	      // primal
	      firstFree = iColumn;
	    } else if (fabs(value)>1.0e2*relaxedTolerance) {
	      // dual with dj
	      firstFree = iColumn;
	    }
	  }
	}
	// should not be negative
	if (value<0.0) {
	  value = - value;
	  if (value>columnDualInfeasibility_) {
	    columnDualInfeasibility_=value;
	    columnDualSequence_=iColumn;
	  }
	  if (value>dualTolerance_) {
	    if (getColumnStatus(iColumn) != isFree) {
	      numberDualInfeasibilitiesWithoutFree_ ++;
	      sumDualInfeasibilities_ += value-dualTolerance_;
	      if (value>relaxedTolerance) 
		sumOfRelaxedDualInfeasibilities_ += value-relaxedTolerance;
	      numberDualInfeasibilities_ ++;
	    } else {
	      // free so relax a lot
	      value *= 0.01;
	      if (value>dualTolerance_) {
		sumDualInfeasibilities_ += value-dualTolerance_;
		if (value>relaxedTolerance) 
		  sumOfRelaxedDualInfeasibilities_ += value-relaxedTolerance;
		numberDualInfeasibilities_ ++;
	      }
	    }
	    // maybe we can make feasible by increasing tolerance
	    if (distanceUp<largeValue_) {
	      if (distanceUp>primalToleranceToGetOptimal_)
		primalToleranceToGetOptimal_=distanceUp;
	    } else {
	      //gap too big for any tolerance
	      remainingDualInfeasibility_=
		max(remainingDualInfeasibility_,value);
	    }
	  }
	}
      }
      if (distanceDown>primalTolerance_) {
	double value = reducedCostWork_[iColumn];
	// should not be positive
	if (value>0.0) {
	  if (value>columnDualInfeasibility_) {
	    columnDualInfeasibility_=value;
	    columnDualSequence_=iColumn;
	  }
	  if (value>dualTolerance_) {
	    sumDualInfeasibilities_ += value-dualTolerance_;
	    if (value>relaxedTolerance) 
	      sumOfRelaxedDualInfeasibilities_ += value-relaxedTolerance;
	    numberDualInfeasibilities_ ++;
	    if (getColumnStatus(iColumn) != isFree) 
	      numberDualInfeasibilitiesWithoutFree_ ++;
	    // maybe we can make feasible by increasing tolerance
	    if (distanceDown<largeValue_&&
		distanceDown>primalToleranceToGetOptimal_)
	      primalToleranceToGetOptimal_=distanceDown;
	  }
	}
      }
    }
  }
  for (iRow=0;iRow<numberRows_;iRow++) {
    if (getRowStatus(iRow) != basic) {
      // not basic
      double distanceUp = rowUpperWork_[iRow]-rowActivityWork_[iRow];
      double distanceDown = rowActivityWork_[iRow] -rowLowerWork_[iRow];
      if (distanceUp>primalTolerance_) {
	double value = rowReducedCost_[iRow];
	if (!flagged(iRow+numberColumns_)) {
	  // Check if "free"
	  if (firstFree<0&&distanceDown>primalTolerance_) {
	    if (algorithm_>0) {
	      // primal
	      firstFree = iRow+numberColumns_;
	    } else if (fabs(value)>1.0e2*relaxedTolerance) {
	      // dual with dj
	      firstFree = iRow+numberColumns_;
	    }
	  }
	}
	// should not be negative
	if (value<0.0) {
	  value = - value;
	  if (value>rowDualInfeasibility_) {
	    rowDualInfeasibility_=value;
	    rowDualSequence_=iRow;
	  }
	  if (value>dualTolerance_) {
	    sumDualInfeasibilities_ += value-dualTolerance_;
	    if (value>relaxedTolerance) 
	      sumOfRelaxedDualInfeasibilities_ += value-relaxedTolerance;
	    numberDualInfeasibilities_ ++;
	    if (getRowStatus(iRow) != isFree) 
	      numberDualInfeasibilitiesWithoutFree_ ++;
	    // maybe we can make feasible by increasing tolerance
	    if (distanceUp<largeValue_) {
	      if (distanceUp>primalToleranceToGetOptimal_)
		primalToleranceToGetOptimal_=distanceUp;
	    } else {
	      //gap too big for any tolerance
	      remainingDualInfeasibility_=
		max(remainingDualInfeasibility_,value);
	    }
	  }
	}
      }
      if (distanceDown>primalTolerance_) {
	double value = rowReducedCost_[iRow];
	// should not be positive
	if (value>0.0) {
	  if (value>rowDualInfeasibility_) {
	    rowDualInfeasibility_=value;
	    rowDualSequence_=iRow;
	  }
	  if (value>dualTolerance_) {
	    sumDualInfeasibilities_ += value-dualTolerance_;
	    if (value>relaxedTolerance) 
	      sumOfRelaxedDualInfeasibilities_ += value-relaxedTolerance;
	    numberDualInfeasibilities_ ++;
	    if (getRowStatus(iRow) != isFree) 
	      numberDualInfeasibilitiesWithoutFree_ ++;
	    // maybe we can make feasible by increasing tolerance
	    if (distanceDown<largeValue_&&
		distanceDown>primalToleranceToGetOptimal_)
	      primalToleranceToGetOptimal_=distanceDown;
	  }
	}
      }
    }
  }
  if (firstFree>=0) {
    if (algorithm_<0||!numberIterations_)
      firstFree_=firstFree;
  }
}
/*
  Unpacks one column of the matrix into indexed array 
*/
void 
ClpSimplex::unpack(CoinIndexedVector * rowArray) const
{
  rowArray->clear();
  if (sequenceIn_>=numberColumns_) {
    //slack
    rowArray->insert(sequenceIn_-numberColumns_,-1.0);
  } else {
    // column
    matrix_->unpack(this,rowArray,sequenceIn_);
  }
}
void 
ClpSimplex::unpack(CoinIndexedVector * rowArray,int sequence) const
{
  rowArray->clear();
  if (sequence>=numberColumns_) {
    //slack
    rowArray->insert(sequence-numberColumns_,-1.0);
  } else {
    // column
    matrix_->unpack(this,rowArray,sequence);
  }
}
bool
ClpSimplex::createRim(int what,bool makeRowCopy)
{
  bool goodMatrix=true;
  int i;
  if ((what&(16+32))!=0) {
    // move information to work arrays
    if (optimizationDirection_<0.0) {
      // reverse all dual signs
      for (i=0;i<numberColumns_;i++) 
	reducedCost_[i] = -reducedCost_[i];
      for (i=0;i<numberRows_;i++) 
	dual_[i] = -dual_[i];
    }
    // row reduced costs
    if (!dj_) {
      dj_ = new double[numberRows_+numberColumns_];
      reducedCostWork_ = dj_;
      rowReducedCost_ = dj_+numberColumns_;
      memcpy(reducedCostWork_,reducedCost_,numberColumns_*sizeof(double));
      memcpy(rowReducedCost_,dual_,numberRows_*sizeof(double));
    }
    if (!solution_||(what&32)!=0) {
      if (!solution_)
	solution_ = new double[numberRows_+numberColumns_];
      columnActivityWork_ = solution_;
      rowActivityWork_ = solution_+numberColumns_;
      memcpy(columnActivityWork_,columnActivity_,
	     numberColumns_*sizeof(double));
      memcpy(rowActivityWork_,rowActivity_,
	     numberRows_*sizeof(double));
    }
  }
  if ((what&16)!=0) {
    //check matrix
    if (!matrix_->allElementsInRange(this,smallElement_,1.0e20)) {
      problemStatus_=4;
      goodMatrix= false;
    }
    if (makeRowCopy) {
      delete rowCopy_;
      // may return NULL if can't give row copy
      rowCopy_ = matrix_->reverseOrderedCopy();
#ifdef TAKEOUT
      {

	ClpPackedMatrix* rowCopy =
	  dynamic_cast< ClpPackedMatrix*>(rowCopy_);
	const int * column = rowCopy->getIndices();
	const CoinBigIndex * rowStart = rowCopy->getVectorStarts();
	const double * element = rowCopy->getElements();
	int i;
	for (i=133;i<numberRows_;i++) {
	  if (rowStart[i+1]-rowStart[i]==10||rowStart[i+1]-rowStart[i]==15)
	    printf("Row %d has %d elements\n",i,rowStart[i+1]-rowStart[i]);
	}
      }  
#endif
    }
  }
  if ((what&4)!=0) {
    delete [] cost_;
    cost_ = new double[numberColumns_+numberRows_];
    objectiveWork_ = cost_;
    rowObjectiveWork_ = cost_+numberColumns_;
    memcpy(objectiveWork_,objective(),numberColumns_*sizeof(double));
    if (rowObjective_)
      memcpy(rowObjectiveWork_,rowObjective_,numberRows_*sizeof(double));
    else
      memset(rowObjectiveWork_,0,numberRows_*sizeof(double));
  }
  if ((what&1)!=0) {
    delete [] lower_;
    delete [] upper_;
    lower_ = new double[numberColumns_+numberRows_];
    upper_ = new double[numberColumns_+numberRows_];
    rowLowerWork_ = lower_+numberColumns_;
    columnLowerWork_ = lower_;
    rowUpperWork_ = upper_+numberColumns_;
    columnUpperWork_ = upper_;
    memcpy(rowLowerWork_,rowLower_,numberRows_*sizeof(double));
    memcpy(rowUpperWork_,rowUpper_,numberRows_*sizeof(double));
    memcpy(columnLowerWork_,columnLower_,numberColumns_*sizeof(double));
    memcpy(columnUpperWork_,columnUpper_,numberColumns_*sizeof(double));
    // clean up any mismatches on infinity
    for (i=0;i<numberColumns_;i++) {
      if (columnLowerWork_[i]<-1.0e30)
	columnLowerWork_[i] = -COIN_DBL_MAX;
      if (columnUpperWork_[i]>1.0e30)
	columnUpperWork_[i] = COIN_DBL_MAX;
    }
    // clean up any mismatches on infinity
    for (i=0;i<numberRows_;i++) {
      if (rowLowerWork_[i]<-1.0e30)
	rowLowerWork_[i] = -COIN_DBL_MAX;
      if (rowUpperWork_[i]>1.0e30)
	rowUpperWork_[i] = COIN_DBL_MAX;
    }
  }
  // do scaling if needed
  if (scalingFlag_>0&&!rowScale_) {
    if (matrix_->scale(this))
      scalingFlag_=-scalingFlag_; // not scaled after all
  }
  if ((what&4)!=0) {
    double direction = optimizationDirection_;
    // direction is actually scale out not scale in
    if (direction)
      direction = 1.0/direction;
    // but also scale by scale factors
    // not really sure about row scaling
    if (!rowScale_) {
      if (direction!=1.0) {
	for (i=0;i<numberRows_;i++)
	  rowObjectiveWork_[i] *= direction;
	for (i=0;i<numberColumns_;i++)
	  objectiveWork_[i] *= direction;
      }
    } else {
      for (i=0;i<numberRows_;i++)
	rowObjectiveWork_[i] *= direction/rowScale_[i];
      for (i=0;i<numberColumns_;i++)
	objectiveWork_[i] *= direction*columnScale_[i];
    }
  }
  if ((what&(1+32))!=0&&rowScale_) {
    for (i=0;i<numberColumns_;i++) {
      double multiplier = 1.0/columnScale_[i];
      if (columnLowerWork_[i]>-1.0e50)
	columnLowerWork_[i] *= multiplier;
      if (columnUpperWork_[i]<1.0e50)
	columnUpperWork_[i] *= multiplier;
    }
    for (i=0;i<numberRows_;i++) {
      if (rowLowerWork_[i]>-1.0e50)
	rowLowerWork_[i] *= rowScale_[i];
      if (rowUpperWork_[i]<1.0e50)
	rowUpperWork_[i] *= rowScale_[i];
    }
  }
  if ((what&(8+32))!=0&&rowScale_) {
    // on entry
    for (i=0;i<numberColumns_;i++) {
      columnActivityWork_[i] /= columnScale_[i];
      reducedCostWork_[i] *= columnScale_[i];
    }
    for (i=0;i<numberRows_;i++) {
      rowActivityWork_[i] *= rowScale_[i];
      dual_[i] /= rowScale_[i];
      rowReducedCost_[i] = dual_[i];
    }
  }
 
  if ((what&16)!=0) {
    // check rim of problem okay
    if (!sanityCheck())
      goodMatrix=false;
  } 
  // we need to treat matrix as if each element by rowScaleIn and columnScaleout??
  // maybe we need to move scales to SimplexModel for factorization?
  if ((what&8)!=0&&!pivotVariable_) {
    pivotVariable_=new int[numberRows_];
  }

  if ((what&16)!=0&&!rowArray_[2]) {
    // get some arrays
    int iRow,iColumn;
    // these are "indexed" arrays so we always know where nonzeros are
    /**********************************************************
      rowArray_[3] is long enough for rows+columns
    *********************************************************/
    for (iRow=0;iRow<4;iRow++) {
      if (!rowArray_[iRow]) {
	rowArray_[iRow]=new CoinIndexedVector();
	int length =numberRows_+factorization_->maximumPivots();
	if (iRow==3)
	  length += numberColumns_;
	rowArray_[iRow]->reserve(length);
      }
    }
    
    for (iColumn=0;iColumn<2;iColumn++) {
      if (!columnArray_[iColumn]) {
	columnArray_[iColumn]=new CoinIndexedVector();
	columnArray_[iColumn]->reserve(numberColumns_);
      }
    }    
  }
  return goodMatrix;
}
void
ClpSimplex::deleteRim(int getRidOfFactorizationData)
{
  int i;
  if (problemStatus_!=1&&problemStatus_!=2) {
    delete [] ray_;
    ray_=NULL;
  }
  // ray may be null if in branch and bound
  if (rowScale_) {
    for (i=0;i<numberColumns_;i++) {
      columnActivity_[i] = columnActivityWork_[i]*columnScale_[i];
      reducedCost_[i] = reducedCostWork_[i]/columnScale_[i];
    }
    for (i=0;i<numberRows_;i++) {
      rowActivity_[i] = rowActivityWork_[i]/rowScale_[i];
      dual_[i] *= rowScale_[i];
    }
    if (problemStatus_==2) {
      for (i=0;i<numberColumns_;i++) {
	ray_[i] *= columnScale_[i];
      }
    } else if (problemStatus_==1&&ray_) {
      for (i=0;i<numberRows_;i++) {
	ray_[i] *= rowScale_[i];
      }
    }
  } else {
    for (i=0;i<numberColumns_;i++) {
      columnActivity_[i] = columnActivityWork_[i];
      reducedCost_[i] = reducedCostWork_[i];
    }
    for (i=0;i<numberRows_;i++) {
      rowActivity_[i] = rowActivityWork_[i];
    }
  }
  // direction may have been modified by scaling - clean up
  if (optimizationDirection_>0.0) {
    optimizationDirection_ = 1.0;
  }  else if (optimizationDirection_<0.0) {
    optimizationDirection_ = -1.0;
    // and reverse all dual signs
    for (i=0;i<numberColumns_;i++) 
      reducedCost_[i] = -reducedCost_[i];
    for (i=0;i<numberRows_;i++) 
      dual_[i] = -dual_[i];
  }
  // scaling may have been turned off
  scalingFlag_ = abs(scalingFlag_);
  if(getRidOfFactorizationData>=0)
    gutsOfDelete(getRidOfFactorizationData+1);
}
void 
ClpSimplex::setDualBound(double value)
{
  if (value>0.0)
    dualBound_=value;
}
void 
ClpSimplex::setInfeasibilityCost(double value)
{
  if (value>0.0)
    infeasibilityCost_=value;
}
void ClpSimplex::setnumberRefinements( int value) 
{
  if (value>=0&&value<10)
    numberRefinements_=value;
}
// Sets row pivot choice algorithm in dual
void 
ClpSimplex::setDualRowPivotAlgorithm(ClpDualRowPivot & choice)
{
  delete dualRowPivot_;
  dualRowPivot_ = choice.clone(true);
}
// Sets row pivot choice algorithm in dual
void 
ClpSimplex::setPrimalColumnPivotAlgorithm(ClpPrimalColumnPivot & choice)
{
  delete primalColumnPivot_;
  primalColumnPivot_ = choice.clone(true);
}
// Sets or unsets scaling, 0 -off, 1 on, 2 dynamic(later)
void 
ClpSimplex::scaling(int mode)
{
  if (mode>0&&mode<3) {
    scalingFlag_=mode;
  } else if (!mode) {
    scalingFlag_=0;
    delete [] rowScale_;
    rowScale_ = NULL;
    delete [] columnScale_;
    columnScale_ = NULL;
  }
}
// Passes in factorization
void 
ClpSimplex::setFactorization( ClpFactorization & factorization)
{
  delete factorization_;
  factorization_= new ClpFactorization(factorization);
}
void 
ClpSimplex::times(double scalar,
		  const double * x, double * y) const
{
  if (rowScale_)
    matrix_->times(scalar,x,y,rowScale_,columnScale_);
  else
    matrix_->times(scalar,x,y);
}
void 
ClpSimplex::transposeTimes(double scalar,
			   const double * x, double * y) const 
{
  if (rowScale_)
    matrix_->transposeTimes(scalar,x,y,rowScale_,columnScale_);
  else
    matrix_->transposeTimes(scalar,x,y);
}
/* Perturbation:
   -50 to +50 - perturb by this power of ten (-6 sounds good)
   100 - auto perturb if takes too long (1.0e-6 largest nonzero)
   101 - we are perturbed
   102 - don't try perturbing again
   default is 100
*/
void 
ClpSimplex::setPerturbation(int value)
{
  if(value<=100&&value >=-50) {
    perturbation_=value;
  } 
}
// Sparsity on or off
bool 
ClpSimplex::sparseFactorization() const
{
  return factorization_->sparseThreshold()!=0;
}
void 
ClpSimplex::setSparseFactorization(bool value)
{
  if (value) {
    if (!factorization_->sparseThreshold())
      factorization_->goSparse();
  } else {
    factorization_->sparseThreshold(0);
  }
}
/* Tightens primal bounds to make dual faster.  Unless
   fixed, bounds are slightly looser than they could be.
   This is to make dual go faster and is probably not needed
   with a presolve.  Returns non-zero if problem infeasible

   Fudge for branch and bound - put bounds on columns of factor *
   largest value (at continuous) - should improve stability
   in branch and bound on infeasible branches (0.0 is off)
*/
int 
ClpSimplex::tightenPrimalBounds(double factor)
{
  
  // Get a row copy in standard format
  CoinPackedMatrix copy;
  copy.reverseOrderedCopyOf(*matrix());
  // get matrix data pointers
  const int * column = copy.getIndices();
  const CoinBigIndex * rowStart = copy.getVectorStarts();
  const int * rowLength = copy.getVectorLengths(); 
  const double * element = copy.getElements();
  int numberChanged=1,iPass=0;
  double large = largeValue(); // treat bounds > this as infinite
  int numberInfeasible=0;
  int totalTightened = 0;

  double tolerance = primalTolerance();


  // Save column bounds
  double * saveLower = new double [numberColumns_];
  memcpy(saveLower,columnLower_,numberColumns_*sizeof(double));
  double * saveUpper = new double [numberColumns_];
  memcpy(saveUpper,columnUpper_,numberColumns_*sizeof(double));

  int iRow, iColumn;

  // If wanted - tighten column bounds using solution
  if (factor) {
    assert (factor>1.0);
    double largest=0.0;
    for (iColumn=0;iColumn<numberColumns_;iColumn++) {
      if (columnUpper_[iColumn]-columnLower_[iColumn]>tolerance) {
	largest = max(largest,fabs(columnActivity_[iColumn]));
      }
    }
    largest *= factor;
    for (iColumn=0;iColumn<numberColumns_;iColumn++) {
      if (columnUpper_[iColumn]-columnLower_[iColumn]>tolerance) {
	columnUpper_[iColumn] = min(columnUpper_[iColumn],largest);
	columnLower_[iColumn] = max(columnLower_[iColumn],-largest);
      }
    }
  }
#define MAXPASS 10  

  // Loop round seeing if we can tighten bounds
  // Would be faster to have a stack of possible rows
  // and we put altered rows back on stack

  while(numberChanged) {

    numberChanged = 0; // Bounds tightened this pass
    
    if (iPass==MAXPASS) break;
    iPass++;
    
    for (iRow = 0; iRow < numberRows_; iRow++) {

      if (rowLower_[iRow]>-large||rowUpper_[iRow]<large) {

	// possible row
	int infiniteUpper = 0;
	int infiniteLower = 0;
	double maximumUp = 0.0;
	double maximumDown = 0.0;
	double newBound;
	CoinBigIndex rStart = rowStart[iRow];
	CoinBigIndex rEnd = rowStart[iRow]+rowLength[iRow];
	CoinBigIndex j;

	// Compute possible lower and upper ranges

	for (j = rStart; j < rEnd; ++j) {
	  double value=element[j];
	  iColumn = column[j];
	  if (value > 0.0) {
	    if (columnUpper_[iColumn] >= large) {
	      maximumUp = 1e31;
	      ++infiniteUpper;
	    } else {
	      maximumUp += columnUpper_[iColumn] * value;
	    }
	    if (columnLower_[iColumn] <= -large) {
	      maximumDown = -1e31;
	      ++infiniteLower;
	    } else {
	      maximumDown += columnLower_[iColumn] * value;
	    }
	  } else if (value<0.0) {
	    if (columnUpper_[iColumn] >= large) {
	      maximumDown = -1e31;
	      ++infiniteLower;
	    } else {
	      maximumDown += columnUpper_[iColumn] * value;
	    }
	    if (columnLower_[iColumn] <= -large) {
	      maximumUp = 1e31;
	      ++infiniteUpper;
	    } else {
	      maximumUp += columnLower_[iColumn] * value;
	    }
	  }
	}
	if (maximumUp <= rowUpper_[iRow] + tolerance && 
	    maximumDown >= rowLower_[iRow] - tolerance) {

	  // Row is redundant - make totally free
	  rowLower_[iRow]=-COIN_DBL_MAX;
	  rowUpper_[iRow]=COIN_DBL_MAX;

	} else {
	  if (maximumUp < rowLower_[iRow] -tolerance ||
	      maximumDown > rowUpper_[iRow]+tolerance) {
	    // problem is infeasible - exit at once
	    numberInfeasible++;
	    if (handler_->logLevel()>3)
	      printf("a %g %g %g %g\n",maximumUp,rowLower_[iRow],
		     maximumDown,rowUpper_[iRow]);
	    break;
	  }

	  if (infiniteUpper == 0 && rowLower_[iRow] > -large) {
	    for (j = rStart; j < rEnd; ++j) {
	      double value=element[j];
	      iColumn = column[j];
	      if (value > 0.0) {
		if (columnUpper_[iColumn] < large) {
		  newBound = columnUpper_[iColumn] + 
		    (rowLower_[iRow] - maximumUp) / value;
		  if (newBound > columnLower_[iColumn] + 1.0e-12) {
		    // Tighten the lower bound 

		    columnLower_[iColumn] = newBound;
		    ++numberChanged;

		    // check infeasible (relaxed)
		    if (columnUpper_[iColumn] - columnLower_[iColumn] < 
			-100.0*tolerance) {
		      numberInfeasible++;
		      if (handler_->logLevel()>3)
			printf("b %g %g \n",columnLower_[iColumn],columnUpper_[iColumn]);
		    }
		    infiniteLower=1; // skip looking at other bound
		  }
		}
	      } else {
		if (columnLower_[iColumn] > -large) {
		  newBound = columnLower_[iColumn] + 
		    (rowLower_[iRow] - maximumUp) / value;
		  if (newBound < columnUpper_[iColumn] - 1.0e-12) {
		    // Tighten the upper bound 

		    columnUpper_[iColumn] = newBound;
		    ++numberChanged;

		    // check infeasible (relaxed)
		    if (columnUpper_[iColumn] - columnLower_[iColumn] < 
			-100.0*tolerance) {
		      numberInfeasible++;
		      if (handler_->logLevel()>3)
			printf("c %g %g \n",columnLower_[iColumn],columnUpper_[iColumn]);
		    }
		    infiniteLower=1; // skip looking at other bound
		  }
		}
	      }
	    }
	  }
	  
	  // Try other way
	  if (infiniteLower == 0 && rowUpper_[iRow] < large) {
	    for (j = rStart; j < rEnd; ++j) {
	      double value=element[j];
	      iColumn = column[j];
	      if (value < 0.0) {
		if (columnUpper_[iColumn] < large) {
		  newBound = columnUpper_[iColumn] + 
		    (rowUpper_[iRow] - maximumDown) / value;
		  if (newBound > columnLower_[iColumn] + 1.0e-12) {
		    // Tighten the lower bound 

		    columnLower_[iColumn] = newBound;
		    ++numberChanged;

		    // check infeasible (relaxed)
		    if (columnUpper_[iColumn] - columnLower_[iColumn] < 
			-100.0*tolerance) {
		      numberInfeasible++;
		      if (handler_->logLevel()>3)
			printf("d %g %g \n",columnLower_[iColumn],columnUpper_[iColumn]);
		    }
		  }
		} 
	      } else {
		if (columnLower_[iColumn] > -large) {
		  newBound = columnLower_[iColumn] + 
		    (rowUpper_[iRow] - maximumDown) / value;
		  if (newBound < columnUpper_[iColumn] - 1.0e-12) {
		    // Tighten the upper bound 

		    columnUpper_[iColumn] = newBound;
		    ++numberChanged;

		    // check infeasible (relaxed)
		    if (columnUpper_[iColumn] - columnLower_[iColumn] < 
			-100.0*tolerance) {
		      numberInfeasible++;
		      if (handler_->logLevel()>3)
			printf("e %g %g \n",columnLower_[iColumn],columnUpper_[iColumn]);
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
    totalTightened += numberChanged;
    if (numberInfeasible) break;
  }
  if (!numberInfeasible) {
    handler_->message(CLP_SIMPLEX_BOUNDTIGHTEN,messages_)
      <<totalTightened
      <<CoinMessageEol;
    // Set bounds slightly loose
    for (iColumn=0;iColumn<numberColumns_;iColumn++) {
      if (columnUpper_[iColumn]-columnLower_[iColumn]<tolerance) {
	// fix
	if (fabs(columnUpper_[iColumn])<fabs(columnLower_[iColumn])) 
	  columnLower_[iColumn]=columnUpper_[iColumn];
	else
	  columnUpper_[iColumn]=columnLower_[iColumn];
      } else {
	if (columnUpper_[iColumn]<saveUpper[iColumn]) {
	  // relax a bit
	  columnUpper_[iColumn] = min(columnUpper_[iColumn]+100.0*tolerance,
				      saveUpper[iColumn]);
	}
	if (columnLower_[iColumn]>saveLower[iColumn]) {
	  // relax a bit
	  columnLower_[iColumn] = max(columnLower_[iColumn]-100.0*tolerance,
				      saveLower[iColumn]);
	}
      }
    }
  } else {
    handler_->message(CLP_SIMPLEX_INFEASIBILITIES,messages_)
      <<numberInfeasible
      <<CoinMessageEol;
    // restore column bounds
    memcpy(columnLower_,saveLower,numberColumns_*sizeof(double));
    memcpy(columnUpper_,saveUpper,numberColumns_*sizeof(double));
  }
  delete [] saveLower;
  delete [] saveUpper;
  return (numberInfeasible);
}
// dual 
#include "ClpSimplexDual.hpp"
int ClpSimplex::dual (int ifValuesPass )
{
  return ((ClpSimplexDual *) this)->dual(ifValuesPass);
}
// primal 
#include "ClpSimplexPrimal.hpp"
int ClpSimplex::primal (int ifValuesPass )
{
  return ((ClpSimplexPrimal *) this)->primal(ifValuesPass);
}
#ifdef QUADRATIC
#include "ClpSimplexPrimalQuadratic.hpp"
/* Solves quadratic problem using SLP - may be used as crash
   for other algorithms when number of iterations small
*/
int 
ClpSimplex::quadraticSLP(int numberPasses, double deltaTolerance)
{
  return ((ClpSimplexPrimalQuadratic *) this)->primalSLP(numberPasses,deltaTolerance);
}
// Solves quadratic using Wolfe's algorithm - primal
int 
ClpSimplex::quadraticWolfe()
{
  return ((ClpSimplexPrimalQuadratic *) this)->primalWolfe();
}
#endif
/* For strong branching.  On input lower and upper are new bounds
   while on output they are objective function values (>1.0e50 infeasible).
   Return code is 0 if nothing interesting, -1 if infeasible both
   ways and +1 if infeasible one way (check values to see which one(s))
*/
int ClpSimplex::strongBranching(int numberVariables,const int * variables,
				double * newLower, double * newUpper,
				bool stopOnFirstInfeasible,
				bool alwaysFinish)
{
  return ((ClpSimplexDual *) this)->strongBranching(numberVariables,variables,
						    newLower,  newUpper,
						    stopOnFirstInfeasible,
						    alwaysFinish);
}
/* Borrow model.  This is so we dont have to copy large amounts
   of data around.  It assumes a derived class wants to overwrite
   an empty model with a real one - while it does an algorithm.
   This is same as ClpModel one, but sets scaling on etc. */
void 
ClpSimplex::borrowModel(ClpModel & otherModel) 
{
  ClpModel::borrowModel(otherModel);
  createStatus();
  ClpDualRowSteepest steep1;
  setDualRowPivotAlgorithm(steep1);
  ClpPrimalColumnSteepest steep2;
  setPrimalColumnPivotAlgorithm(steep2);
}
typedef struct {
  double optimizationDirection;
  double dblParam[ClpLastDblParam];
  double objectiveValue;
  double dualBound;
  double dualTolerance;
  double primalTolerance;
  double sumDualInfeasibilities;
  double sumPrimalInfeasibilities;
  double infeasibilityCost;
  int numberRows;
  int numberColumns;
  int intParam[ClpLastIntParam];
  int numberIterations;
  int problemStatus;
  int maximumIterations;
  int lengthNames;
  int numberDualInfeasibilities;
  int numberDualInfeasibilitiesWithoutFree;
  int numberPrimalInfeasibilities;
  int numberRefinements;
  int scalingFlag;
  int algorithm;
  unsigned int specialOptions;
  int dualPivotChoice;
  int primalPivotChoice;
  int matrixStorageChoice;
} Clp_scalars;

int outDoubleArray(double * array, int length, FILE * fp)
{
  int numberWritten;
  if (array&&length) {
    numberWritten = fwrite(&length,sizeof(int),1,fp);
    if (numberWritten!=1)
      return 1;
    numberWritten = fwrite(array,sizeof(double),length,fp);
    if (numberWritten!=length)
      return 1;
  } else {
    length = 0;
    numberWritten = fwrite(&length,sizeof(int),1,fp);
    if (numberWritten!=1)
      return 1;
  }
  return 0;
}
// Save model to file, returns 0 if success
int
ClpSimplex::saveModel(const char * fileName)
{
  FILE * fp = fopen(fileName,"wb");
  if (fp) {
    Clp_scalars scalars;
    int i;
    CoinBigIndex numberWritten;
    // Fill in scalars
    scalars.optimizationDirection = optimizationDirection_;
    memcpy(scalars.dblParam, dblParam_,ClpLastDblParam * sizeof(double));
    scalars.objectiveValue = objectiveValue_;
    scalars.dualBound = dualBound_;
    scalars.dualTolerance = dualTolerance_;
    scalars.primalTolerance = primalTolerance_;
    scalars.sumDualInfeasibilities = sumDualInfeasibilities_;
    scalars.sumPrimalInfeasibilities = sumPrimalInfeasibilities_;
    scalars.infeasibilityCost = infeasibilityCost_;
    scalars.numberRows = numberRows_;
    scalars.numberColumns = numberColumns_;
    memcpy(scalars.intParam, intParam_,ClpLastIntParam * sizeof(double));
    scalars.numberIterations = numberIterations_;
    scalars.problemStatus = problemStatus_;
    scalars.maximumIterations = maximumIterations();
    scalars.lengthNames = lengthNames_;
    scalars.numberDualInfeasibilities = numberDualInfeasibilities_;
    scalars.numberDualInfeasibilitiesWithoutFree 
      = numberDualInfeasibilitiesWithoutFree_;
    scalars.numberPrimalInfeasibilities = numberPrimalInfeasibilities_;
    scalars.numberRefinements = numberRefinements_;
    scalars.scalingFlag = scalingFlag_;
    scalars.algorithm = algorithm_;
    scalars.specialOptions = specialOptions_;
    scalars.dualPivotChoice = dualRowPivot_->type();
    scalars.primalPivotChoice = primalColumnPivot_->type();
    scalars.matrixStorageChoice = matrix_->type();

    // put out scalars
    numberWritten = fwrite(&scalars,sizeof(Clp_scalars),1,fp);
    if (numberWritten!=1)
      return 1;
    // strings
    CoinBigIndex length;
    for (i=0;i<ClpLastStrParam;i++) {
      length = strParam_[i].size();
      numberWritten = fwrite(&length,sizeof(int),1,fp);
      if (numberWritten!=1)
	return 1;
      numberWritten = fwrite(strParam_[i].c_str(),length,1,fp);
      if (numberWritten!=1)
	return 1;
    }
    // arrays - in no particular order
    if (outDoubleArray(rowActivity_,numberRows_,fp))
	return 1;
    if (outDoubleArray(columnActivity_,numberColumns_,fp))
	return 1;
    if (outDoubleArray(dual_,numberRows_,fp))
	return 1;
    if (outDoubleArray(reducedCost_,numberColumns_,fp))
	return 1;
    if (outDoubleArray(rowLower_,numberRows_,fp))
	return 1;
    if (outDoubleArray(rowUpper_,numberRows_,fp))
	return 1;
    if (outDoubleArray(objective(),numberColumns_,fp))
	return 1;
    if (outDoubleArray(rowObjective_,numberRows_,fp))
	return 1;
    if (outDoubleArray(columnLower_,numberColumns_,fp))
	return 1;
    if (outDoubleArray(columnUpper_,numberColumns_,fp))
	return 1;
    if (ray_) {
      if (problemStatus_==1)
	if (outDoubleArray(ray_,numberRows_,fp))
	  return 1;
      else if (problemStatus_==2)
	if (outDoubleArray(ray_,numberColumns_,fp))
	  return 1;
      else
	if (outDoubleArray(NULL,0,fp))
	  return 1;
    } else {
      if (outDoubleArray(NULL,0,fp))
	return 1;
    }
    if (status_&&(numberRows_+numberColumns_)>0) {
      length = numberRows_+numberColumns_;
      numberWritten = fwrite(&length,sizeof(int),1,fp);
      if (numberWritten!=1)
	return 1;
      numberWritten = fwrite(status_,sizeof(char),length, fp);
      if (numberWritten!=length)
	return 1;
    } else {
      length = 0;
      numberWritten = fwrite(&length,sizeof(int),1,fp);
      if (numberWritten!=1)
	return 1;
    }
    if (lengthNames_) {
      assert (numberRows_ == (int) rowNames_.size());
      for (i=0;i<numberRows_;i++) {
	length = rowNames_[i].size();
	numberWritten = fwrite(&length,sizeof(int),1,fp);
	if (numberWritten!=1)
	  return 1;
	numberWritten = fwrite(rowNames_[i].c_str(),length,1,fp);
	if (numberWritten!=1)
	  return 1;
      }
      assert (numberColumns_ == (int) columnNames_.size());
      for (i=0;i<numberColumns_;i++) {
	length = columnNames_[i].size();
	numberWritten = fwrite(&length,sizeof(int),1,fp);
	if (numberWritten!=1)
	  return 1;
	numberWritten = fwrite(columnNames_[i].c_str(),length,1,fp);
	if (numberWritten!=1)
	  return 1;
      }
    }
    // just standard type at present
    assert (matrix_->type()==1);
    assert (matrix_->getNumCols() == numberColumns_);
    assert (matrix_->getNumRows() == numberRows_);
    // we are going to save with gaps
    length = matrix_->getVectorStarts()[numberColumns_-1]
      + matrix_->getVectorLengths()[numberColumns_-1];
    numberWritten = fwrite(&length,sizeof(int),1,fp);
    if (numberWritten!=1)
      return 1;
    numberWritten = fwrite(matrix_->getElements(),
			   sizeof(double),length,fp);
    if (numberWritten!=length)
      return 1;
    numberWritten = fwrite(matrix_->getIndices(),
			   sizeof(int),length,fp);
    if (numberWritten!=length)
      return 1;
    numberWritten = fwrite(matrix_->getVectorStarts(),
			   sizeof(int),numberColumns_,fp);
    if (numberWritten!=numberColumns_)
      return 1;
    numberWritten = fwrite(matrix_->getVectorLengths(),
			   sizeof(int),numberColumns_,fp);
    if (numberWritten!=numberColumns_)
      return 1;
    // finished
    fclose(fp);
    return 0;
  } else {
    return -1;
  }
}

int inDoubleArray(double * &array, int length, FILE * fp)
{
  int numberRead;
  int length2;
  numberRead = fread(&length2,sizeof(int),1,fp);
  if (numberRead!=1)
    return 1;
  if (length2) {
    // lengths must match
    if (length!=length2)
      return 2;
    array = new double[length];
    numberRead = fread(array,sizeof(double),length,fp);
    if (numberRead!=length)
      return 1;
  } 
  return 0;
}
/* Restore model from file, returns 0 if success,
   deletes current model */
int 
ClpSimplex::restoreModel(const char * fileName)
{
  FILE * fp = fopen(fileName,"rb");
  if (fp) {
    // Get rid of current model
    ClpModel::gutsOfDelete();
    gutsOfDelete(0);
    int i;
    for (i=0;i<6;i++) {
      rowArray_[i]=NULL;
      columnArray_[i]=NULL;
    }
    // get an empty factorization so we can set tolerances etc
    factorization_ = new ClpFactorization();
    // Say sparse
    factorization_->sparseThreshold(1);
    Clp_scalars scalars;
    CoinBigIndex numberRead;

    // get scalars
    numberRead = fread(&scalars,sizeof(Clp_scalars),1,fp);
    if (numberRead!=1)
      return 1;
    // Fill in scalars
    optimizationDirection_ = scalars.optimizationDirection;
    memcpy(dblParam_, scalars.dblParam, ClpLastDblParam * sizeof(double));
    objectiveValue_ = scalars.objectiveValue;
    dualBound_ = scalars.dualBound;
    dualTolerance_ = scalars.dualTolerance;
    primalTolerance_ = scalars.primalTolerance;
    sumDualInfeasibilities_ = scalars.sumDualInfeasibilities;
    sumPrimalInfeasibilities_ = scalars.sumPrimalInfeasibilities;
    infeasibilityCost_ = scalars.infeasibilityCost;
    numberRows_ = scalars.numberRows;
    numberColumns_ = scalars.numberColumns;
    memcpy(intParam_, scalars.intParam,ClpLastIntParam * sizeof(double));
    numberIterations_ = scalars.numberIterations;
    problemStatus_ = scalars.problemStatus;
    setMaximumIterations(scalars.maximumIterations);
    lengthNames_ = scalars.lengthNames;
    numberDualInfeasibilities_ = scalars.numberDualInfeasibilities;
    numberDualInfeasibilitiesWithoutFree_ 
      = scalars.numberDualInfeasibilitiesWithoutFree;
    numberPrimalInfeasibilities_ = scalars.numberPrimalInfeasibilities;
    numberRefinements_ = scalars.numberRefinements;
    scalingFlag_ = scalars.scalingFlag;
    algorithm_ = scalars.algorithm;
    specialOptions_ = scalars.specialOptions;
    // strings
    CoinBigIndex length;
    for (i=0;i<ClpLastStrParam;i++) {
      numberRead = fread(&length,sizeof(int),1,fp);
      if (numberRead!=1)
	return 1;
      char * array = new char[length+1];
      numberRead = fread(array,length,1,fp);
      if (numberRead!=1)
	return 1;
      array[length]='\0';
      strParam_[i]=array;
      delete [] array;
    }
    // arrays - in no particular order
    if (inDoubleArray(rowActivity_,numberRows_,fp))
	return 1;
    if (inDoubleArray(columnActivity_,numberColumns_,fp))
	return 1;
    if (inDoubleArray(dual_,numberRows_,fp))
	return 1;
    if (inDoubleArray(reducedCost_,numberColumns_,fp))
	return 1;
    if (inDoubleArray(rowLower_,numberRows_,fp))
	return 1;
    if (inDoubleArray(rowUpper_,numberRows_,fp))
	return 1;
    double * objective;
    if (inDoubleArray(objective,numberColumns_,fp))
	return 1;
    delete objective_;
    objective_ = new ClpLinearObjective(objective,numberColumns_);
    delete [] objective;
    if (inDoubleArray(rowObjective_,numberRows_,fp))
	return 1;
    if (inDoubleArray(columnLower_,numberColumns_,fp))
	return 1;
    if (inDoubleArray(columnUpper_,numberColumns_,fp))
	return 1;
    if (problemStatus_==1) {
      if (inDoubleArray(ray_,numberRows_,fp))
	return 1;
    } else if (problemStatus_==2) {
      if (inDoubleArray(ray_,numberColumns_,fp))
	return 1;
    } else {
      // ray should be null
      numberRead = fread(&length,sizeof(int),1,fp);
      if (numberRead!=1)
	return 1;
      if (length)
	return 2;
    }
    delete [] status_;
    status_=NULL;
    // status region
    numberRead = fread(&length,sizeof(int),1,fp);
    if (numberRead!=1)
	return 1;
    if (length) {
      if (length!=numberRows_+numberColumns_)
	return 1;
      status_ = new char unsigned[length];
      numberRead = fread(status_,sizeof(char),length, fp);
      if (numberRead!=length)
	return 1;
    }
    if (lengthNames_) {
      char * array = new char[lengthNames_+1];
      rowNames_.resize(0);
      for (i=0;i<numberRows_;i++) {
	numberRead = fread(&length,sizeof(int),1,fp);
	if (numberRead!=1)
	  return 1;
	numberRead = fread(array,length,1,fp);
	if (numberRead!=1)
	  return 1;
	rowNames_[i]=array;
      }
      columnNames_.resize(0);
      for (i=0;i<numberColumns_;i++) {
	numberRead = fread(&length,sizeof(int),1,fp);
	if (numberRead!=1)
	  return 1;
	numberRead = fread(array,length,1,fp);
	if (numberRead!=1)
	  return 1;
	columnNames_[i]=array;
      }
      delete [] array;
    }
    // Pivot choices
    assert(scalars.dualPivotChoice>0&&(scalars.dualPivotChoice&63)<3);
    delete dualRowPivot_;
    switch ((scalars.dualPivotChoice&63)) {
    case 1:
      // Dantzig
      dualRowPivot_ = new ClpDualRowDantzig();
      break;
    case 2:
      // Steepest - use mode
      dualRowPivot_ = new ClpDualRowSteepest(scalars.dualPivotChoice>>6);
      break;
    default:
      abort();
    }
    assert(scalars.primalPivotChoice>0&&(scalars.primalPivotChoice&63)<3);
    delete primalColumnPivot_;
    switch ((scalars.primalPivotChoice&63)) {
    case 1:
      // Dantzig
      primalColumnPivot_ = new ClpPrimalColumnDantzig();
      break;
    case 2:
      // Steepest - use mode
      primalColumnPivot_ 
	= new ClpPrimalColumnSteepest(scalars.primalPivotChoice>>6);
      break;
    default:
      abort();
    }
    assert(scalars.matrixStorageChoice==1);
    delete matrix_;
    // get arrays
    numberRead = fread(&length,sizeof(int),1,fp);
    if (numberRead!=1)
      return 1;
    double * elements = new double[length];
    int * indices = new int[length];
    CoinBigIndex * starts = new CoinBigIndex[numberColumns_];
    int * lengths = new int[numberColumns_];
    numberRead = fread(elements, sizeof(double),length,fp);
    if (numberRead!=length)
      return 1;
    numberRead = fread(indices, sizeof(int),length,fp);
    if (numberRead!=length)
      return 1;
    numberRead = fread(starts, sizeof(int),numberColumns_,fp);
    if (numberRead!=numberColumns_)
      return 1;
    numberRead = fread(lengths, sizeof(int),numberColumns_,fp);
    if (numberRead!=numberColumns_)
      return 1;
    // assign matrix
    CoinPackedMatrix * matrix = new CoinPackedMatrix();
    matrix->assignMatrix(true, numberRows_, numberColumns_,
			 length, elements, indices, starts, lengths);
    // and transfer to Clp
    matrix_ = new ClpPackedMatrix(matrix);
    // finished
    fclose(fp);
    return 0;
  } else {
    return -1;
  }
  return 0;
}
// value of incoming variable (in Dual)
double 
ClpSimplex::valueIncomingDual() const
{
  // Need value of incoming for list of infeasibilities as may be infeasible
  double valueIncoming = (dualOut_/alpha_)*directionOut_;
  if (directionIn_==-1)
    valueIncoming = upperIn_-valueIncoming;
  else
    valueIncoming = lowerIn_-valueIncoming;
  return valueIncoming;
}
//#############################################################################

ClpSimplexProgress::ClpSimplexProgress () 
{
  int i;
  for (i=0;i<CLP_PROGRESS;i++) {
    objective_[i] = 0.0;
    infeasibility_[i] = -1.0; // set to an impossible value
    numberInfeasibilities_[i]=-1; 
  }
  numberTimes_ = 0;
  numberBadTimes_ = 0;
  model_ = NULL;
}


//-----------------------------------------------------------------------------

ClpSimplexProgress::~ClpSimplexProgress ()
{
}
// Copy constructor. 
ClpSimplexProgress::ClpSimplexProgress(const ClpSimplexProgress &rhs) 
{
  int i;
  for (i=0;i<CLP_PROGRESS;i++) {
    objective_[i] = rhs.objective_[i];
    infeasibility_[i] = rhs.infeasibility_[i];
    numberInfeasibilities_[i]=rhs.numberInfeasibilities_[i]; 
  }
  numberTimes_ = rhs.numberTimes_;
  numberBadTimes_ = rhs.numberBadTimes_;
  model_ = rhs.model_;
}
// Copy constructor.from model
ClpSimplexProgress::ClpSimplexProgress(ClpSimplex * model) 
{
  int i;
  for (i=0;i<CLP_PROGRESS;i++) {
    objective_[i] = 0.0;
    infeasibility_[i] = -1.0; // set to an impossible value
    numberInfeasibilities_[i]=-1; 
  }
  numberTimes_ = 0;
  numberBadTimes_ = 0;
  model_ = model;
}
// Assignment operator. This copies the data
ClpSimplexProgress & 
ClpSimplexProgress::operator=(const ClpSimplexProgress & rhs)
{
  if (this != &rhs) {
    int i;
    for (i=0;i<CLP_PROGRESS;i++) {
      objective_[i] = rhs.objective_[i];
      infeasibility_[i] = rhs.infeasibility_[i];
      numberInfeasibilities_[i]=rhs.numberInfeasibilities_[i]; 
    }
    numberTimes_ = rhs.numberTimes_;
    numberBadTimes_ = rhs.numberBadTimes_;
    model_ = rhs.model_;
  }
  return *this;
}
// Seems to be something odd about exact comparison of doubles on linux
static bool equalDouble(double value1, double value2)
{
  assert(sizeof(unsigned int)*2==sizeof(double));
  unsigned int *i1 = (unsigned int *) &value1;
  unsigned int *i2 = (unsigned int *) &value2;
  return (i1[0]==i2[0]&&i1[1]==i2[1]);
}
int
ClpSimplexProgress::looping()
{
  if (!model_)
    return -1;
  double objective = model_->objectiveValue();
  double infeasibility;
  int numberInfeasibilities;
  if (model_->algorithm()<0) {
    // dual
    infeasibility = model_->sumPrimalInfeasibilities();
    numberInfeasibilities = model_->numberPrimalInfeasibilities();
  } else {
    //primal
    infeasibility = model_->sumDualInfeasibilities();
    numberInfeasibilities = model_->numberDualInfeasibilities();
  }
  int i;
  int numberMatched=0;
  int matched=0;

  for (i=0;i<CLP_PROGRESS;i++) {
    bool matchedOnObjective = equalDouble(objective,objective_[i]);
    bool matchedOnInfeasibility = equalDouble(infeasibility,infeasibility_[i]);
    bool matchedOnInfeasibilities = 
      (numberInfeasibilities==numberInfeasibilities_[i]);

    if (matchedOnObjective&&matchedOnInfeasibility&&matchedOnInfeasibilities) {
      matched |= (1<<i);
      numberMatched++;
      // here mainly to get over compiler bug?
      if (model_->messageHandler()->logLevel()>10)
	printf("%d %d %d %d %d loop check\n",i,numberMatched,
	       matchedOnObjective, matchedOnInfeasibility, 
	       matchedOnInfeasibilities);
    }
    if (i) {
      objective_[i-1] = objective_[i];
      infeasibility_[i-1] = infeasibility_[i];
      numberInfeasibilities_[i-1]=numberInfeasibilities_[i]; 
    }
  }
  objective_[CLP_PROGRESS-1] = objective;
  infeasibility_[CLP_PROGRESS-1] = infeasibility;
  numberInfeasibilities_[CLP_PROGRESS-1]=numberInfeasibilities;
  if (model_->progressFlag())
    numberMatched=0;
  numberTimes_++;
  if (numberTimes_<10)
    numberMatched=0;
  // skip if just last time as may be checking something
  if (matched==(1<<(CLP_PROGRESS-1)))
    numberMatched=0;
  if (numberMatched) {
    model_->messageHandler()->message(CLP_POSSIBLELOOP,model_->messages())
      <<numberMatched
      <<matched
      <<numberTimes_
      <<CoinMessageEol;
    numberBadTimes_++;
    if (numberBadTimes_<10) {
      // make factorize every iteration
      model_->forceFactorization(1);
      if (model_->algorithm()<0) {
	// dual - change tolerance
	model_->setCurrentDualTolerance(model_->currentDualTolerance()*1.05);
	// if infeasible increase dual bound
	if (model_->dualBound()<1.0e19) {
	  model_->setDualBound(model_->dualBound()*1.1);
	}
      } else {
	// primal - change tolerance
	model_->setCurrentPrimalTolerance(model_->currentPrimalTolerance()*1.05);
	// if infeasible increase infeasibility cost
	if (model_->infeasibilityCost()<1.0e19) {
	  model_->setInfeasibilityCost(model_->infeasibilityCost()*1.1);
	}
      }
      return -2;
    } else {
      model_->messageHandler()->message(CLP_LOOP,model_->messages())
	<<CoinMessageEol;
      // look at solution and maybe declare victory
      if (infeasibility<1.0e-4)
	return 0;
      else
	return 3;
    }
  }
  return -1;
}
// Sanity check on input data - returns true if okay
bool 
ClpSimplex::sanityCheck()
{
  // bad if empty
  if (!numberRows_||!numberColumns_||!matrix_->getNumElements()) {
    handler_->message(CLP_EMPTY_PROBLEM,messages_)
      <<numberRows_
      <<numberColumns_
      <<matrix_->getNumElements()
      <<CoinMessageEol;
    problemStatus_=4;
    return false;
  }
  int numberBad ;
  double largestBound, smallestBound, minimumGap;
  double smallestObj, largestObj;
  int firstBad;
  int modifiedBounds=0;
  int i;
  numberBad=0;
  firstBad=-1;
  minimumGap=1.0e100;
  smallestBound=1.0e100;
  largestBound=0.0;
  smallestObj=1.0e100;
  largestObj=0.0;
  // If bounds are too close - fix
  double fixTolerance = 1.1*primalTolerance_;
  for (i=numberColumns_;i<numberColumns_+numberRows_;i++) {
    double value;
    value = fabs(cost_[i]);
    if (value>1.0e50) {
      numberBad++;
      if (firstBad<0)
	firstBad=i;
    } else if (value) {
      if (value>largestObj)
	largestObj=value;
      if (value<smallestObj)
	smallestObj=value;
    }
    value=upper_[i]-lower_[i];
    if (value<-primalTolerance_) {
      numberBad++;
      if (firstBad<0)
	firstBad=i;
    } else if (value<=fixTolerance) {
      if (value) {
	// modify
	upper_[i] = lower_[i];
	modifiedBounds++;
      }
    } else {
      if (value<minimumGap)
	minimumGap=value;
    }
    if (lower_[i]>-1.0e100&&lower_[i]) {
      value = fabs(lower_[i]);
      if (value>largestBound)
	largestBound=value;
      if (value<smallestBound)
	smallestBound=value;
    }
    if (upper_[i]<1.0e100&&upper_[i]) {
      value = fabs(upper_[i]);
      if (value>largestBound)
	largestBound=value;
      if (value<smallestBound)
	smallestBound=value;
    }
  }
  if (largestBound)
    handler_->message(CLP_RIMSTATISTICS3,messages_)
      <<smallestBound
      <<largestBound
      <<minimumGap
      <<CoinMessageEol;
  minimumGap=1.0e100;
  smallestBound=1.0e100;
  largestBound=0.0;
  for (i=0;i<numberColumns_;i++) {
    double value;
    value = fabs(cost_[i]);
    if (value>1.0e50) {
      numberBad++;
      if (firstBad<0)
	firstBad=i;
    } else if (value) {
      if (value>largestObj)
	largestObj=value;
      if (value<smallestObj)
	smallestObj=value;
    }
    value=upper_[i]-lower_[i];
    if (value<-primalTolerance_) {
      numberBad++;
      if (firstBad<0)
	firstBad=i;
    } else if (value<=fixTolerance) {
      if (value) {
	// modify
	upper_[i] = lower_[i];
	modifiedBounds++;
      }
    } else {
      if (value<minimumGap)
	minimumGap=value;
    }
    if (lower_[i]>-1.0e100&&lower_[i]) {
      value = fabs(lower_[i]);
      if (value>largestBound)
	largestBound=value;
      if (value<smallestBound)
	smallestBound=value;
    }
    if (upper_[i]<1.0e100&&upper_[i]) {
      value = fabs(upper_[i]);
      if (value>largestBound)
	largestBound=value;
      if (value<smallestBound)
	smallestBound=value;
    }
  }
  char rowcol[]={'R','C'};
  if (numberBad) {
    handler_->message(CLP_BAD_BOUNDS,messages_)
      <<numberBad
      <<rowcol[isColumn(firstBad)]<<sequenceWithin(firstBad)
      <<CoinMessageEol;
    problemStatus_=4;
    return false;
  }
  if (modifiedBounds)
    handler_->message(CLP_MODIFIEDBOUNDS,messages_)
      <<modifiedBounds
      <<CoinMessageEol;
  handler_->message(CLP_RIMSTATISTICS1,messages_)
    <<smallestObj
    <<largestObj
    <<CoinMessageEol;
  if (largestBound)
    handler_->message(CLP_RIMSTATISTICS2,messages_)
      <<smallestBound
      <<largestBound
      <<minimumGap
      <<CoinMessageEol;
  return true;
}
// Set up status array (for OsiClp)
void 
ClpSimplex::createStatus() 
{
  if(!status_)
    status_ = new unsigned char [numberColumns_+numberRows_];
  memset(status_,0,(numberColumns_+numberRows_)*sizeof(char));
  int i;
  // set column status to one nearest zero
  for (i=0;i<numberColumns_;i++) {
#if 0
    if (columnLower_[i]>=0.0) {
      setColumnStatus(i,atLowerBound);
    } else if (columnUpper_[i]<=0.0) {
      setColumnStatus(i,atUpperBound);
    } else if (columnLower_[i]<-1.0e20&&columnUpper_[i]>1.0e20) {
      // free
      setColumnStatus(i,isFree);
    } else if (fabs(columnLower_[i])<fabs(columnUpper_[i])) {
      setColumnStatus(i,atLowerBound);
    } else {
      setColumnStatus(i,atUpperBound);
    }
#else
    setColumnStatus(i,atLowerBound);
#endif
  }
  for (i=0;i<numberRows_;i++) {
    setRowStatus(i,basic);
  }
}
/* Loads a problem (the constraints on the
   rows are given by lower and upper bounds). If a pointer is 0 then the
   following values are the default:
   <ul>
   <li> <code>colub</code>: all columns have upper bound infinity
   <li> <code>collb</code>: all columns have lower bound 0 
   <li> <code>rowub</code>: all rows have upper bound infinity
   <li> <code>rowlb</code>: all rows have lower bound -infinity
   <li> <code>obj</code>: all variables have 0 objective coefficient
   </ul>
*/
void 
ClpSimplex::loadProblem (  const ClpMatrixBase& matrix,
		    const double* collb, const double* colub,   
		    const double* obj,
		    const double* rowlb, const double* rowub,
		    const double * rowObjective)
{
  ClpModel::loadProblem(matrix, collb, colub, obj, rowlb, rowub,
			rowObjective);
  createStatus();
}
void 
ClpSimplex::loadProblem (  const CoinPackedMatrix& matrix,
		    const double* collb, const double* colub,   
		    const double* obj,
		    const double* rowlb, const double* rowub,
		    const double * rowObjective)
{
  ClpModel::loadProblem(matrix, collb, colub, obj, rowlb, rowub,
			rowObjective);
  createStatus();
}

/* Just like the other loadProblem() method except that the matrix is
   given in a standard column major ordered format (without gaps). */
void 
ClpSimplex::loadProblem (  const int numcols, const int numrows,
		    const CoinBigIndex* start, const int* index,
		    const double* value,
		    const double* collb, const double* colub,   
		    const double* obj,
		    const double* rowlb, const double* rowub,
		    const double * rowObjective)
{
  ClpModel::loadProblem(numcols, numrows, start, index, value,
			  collb, colub, obj, rowlb, rowub,
			  rowObjective);
  createStatus();
}
void 
ClpSimplex::loadProblem (  const int numcols, const int numrows,
			   const CoinBigIndex* start, const int* index,
			   const double* value,const int * length,
			   const double* collb, const double* colub,   
			   const double* obj,
			   const double* rowlb, const double* rowub,
			   const double * rowObjective)
{
  ClpModel::loadProblem(numcols, numrows, start, index, value, length,
			  collb, colub, obj, rowlb, rowub,
			  rowObjective);
  createStatus();
}
// Read an mps file from the given filename
int 
ClpSimplex::readMps(const char *filename,
	    bool keepNames,
	    bool ignoreErrors)
{
  int status = ClpModel::readMps(filename,keepNames,ignoreErrors);
  createStatus();
  return status;
}
// Just check solution (for external use)
void 
ClpSimplex::checkSolution()
{
  // put in standard form
  createRim(7+8+16);
  dualTolerance_=dblParam_[ClpDualTolerance];
  primalTolerance_=dblParam_[ClpPrimalTolerance];
  checkPrimalSolution( rowActivityWork_, columnActivityWork_);
  checkDualSolution();
#ifdef CLP_DEBUG
  int i;
  double value=0.0;
  for (i=0;i<numberRows_+numberColumns_;i++)
    value += dj_[i]*solution_[i];
  printf("dual value %g, primal %g\n",value,objectiveValue());
#endif
  // release extra memory
  deleteRim(0);
}
/* Crash - at present just aimed at dual, returns
   -2 if dual preferred and crash basis created
   -1 if dual preferred and all slack basis preferred
   0 if basis going in was not all slack
   1 if primal preferred and all slack basis preferred
   2 if primal preferred and crash basis created.
   
   if gap between bounds <="gap" variables can be flipped
   
   If "pivot" is
   0 No pivoting (so will just be choice of algorithm)
   1 Simple pivoting e.g. gub
   2 Mini iterations
*/
int 
ClpSimplex::crash(double gap,int pivot)
{
  assert(!rowObjective_); // not coded
  int iColumn;
  int numberBad=0;
  int numberBasic=0;
  double dualTolerance=dblParam_[ClpDualTolerance];
  //double primalTolerance=dblParam_[ClpPrimalTolerance];

  // If no basis then make all slack one
  if (!status_)
    createStatus();
  
  for (iColumn=0;iColumn<numberColumns_;iColumn++) {
    if (getColumnStatus(iColumn)==basic)
      numberBasic++;
  }
  if (numberBasic) {
    // not all slack
    return 0;
  } else {
    double * dj = new double [numberColumns_];
    double * solution = columnActivity_;
    const double * linearObjective = objective();
    //double objectiveValue=0.0;
    int iColumn;
    for (iColumn=0;iColumn<numberColumns_;iColumn++)
      dj[iColumn] = optimizationDirection_*linearObjective[iColumn];
    for (iColumn=0;iColumn<numberColumns_;iColumn++) {
      // assume natural place is closest to zero
      double lowerBound = columnLower_[iColumn];
      double upperBound = columnUpper_[iColumn];
      if (lowerBound>-1.0e20||upperBound<1.0e20) {
	bool atLower;
	if (fabs(upperBound)<fabs(lowerBound)) {
	  atLower=false;
	  setColumnStatus(iColumn,atUpperBound);
	  solution[iColumn]=upperBound;
	} else {
	  atLower=true;
	  setColumnStatus(iColumn,atLowerBound);
	  solution[iColumn]=lowerBound;
	}
	if (dj[iColumn]<0.0) {
	  // should be at upper bound
	  if (atLower) {
	    // can we flip
	    if (upperBound-lowerBound<=gap) {
	      columnActivity_[iColumn]=upperBound;
	      setColumnStatus(iColumn,atUpperBound);
	    } else if (dj[iColumn]<-dualTolerance) {
	      numberBad++;
	    }
	  }
	} else if (dj[iColumn]>0.0) {
	  // should be at lower bound
	  if (!atLower) {
	    // can we flip
	    if (upperBound-lowerBound<=gap) {
	      columnActivity_[iColumn]=lowerBound;
	      setColumnStatus(iColumn,atLowerBound);
	    } else if (dj[iColumn]>dualTolerance) {
	      numberBad++;
	    }
	  }
	}
      } else {
	// free
	setColumnStatus(iColumn,isFree);
	if (fabs(dj[iColumn])>dualTolerance) 
	  numberBad++;
      }
    }
    if (numberBad||pivot) {
      if (!pivot) {
	delete [] dj;
	return 1;
      } else {
	// see if can be made dual feasible with gubs etc
	double * pi = new double[numberRows_];
	memset (pi,0,numberRows_*sizeof(double));
	int * way = new int[numberColumns_];
	int numberIn = 0;
	
	// Get column copy
	CoinPackedMatrix * columnCopy = matrix();
	// Get a row copy in standard format
	CoinPackedMatrix copy;
	copy.reverseOrderedCopyOf(*columnCopy);
	// get matrix data pointers
	const int * column = copy.getIndices();
	const CoinBigIndex * rowStart = copy.getVectorStarts();
	const int * rowLength = copy.getVectorLengths(); 
	const double * elementByRow = copy.getElements();
	//const int * row = columnCopy->getIndices();
	//const CoinBigIndex * columnStart = columnCopy->getVectorStarts();
	//const int * columnLength = columnCopy->getVectorLengths(); 
	//const double * element = columnCopy->getElements();
	
	
	// if equality row and bounds mean artificial in basis bad
	// then do anyway
	
	for (iColumn=0;iColumn<numberColumns_;iColumn++) {
	  // - if we want to reduce dj, + if we want to increase
	  int thisWay = 100;
	  double lowerBound = columnLower_[iColumn];
	  double upperBound = columnUpper_[iColumn];
	  if (upperBound>lowerBound) {
	    switch(getColumnStatus(iColumn)) {
	      
	    case basic:
	      thisWay=0;
	    case ClpSimplex::isFixed:
	      break;
	    case isFree:
	    case superBasic:
	      if (dj[iColumn]<-dualTolerance) 
		thisWay = 1;
	      else if (dj[iColumn]>dualTolerance) 
		thisWay = -1;
	      else
		thisWay =0;
	      break;
	    case atUpperBound:
	      if (dj[iColumn]>dualTolerance) 
		thisWay = -1;
	      else if (dj[iColumn]<-dualTolerance) 
		thisWay = -3;
	      else
		thisWay = -2;
	      break;
	    case atLowerBound:
	      if (dj[iColumn]<-dualTolerance) 
		thisWay = 1;
	      else if (dj[iColumn]>dualTolerance) 
		thisWay = 3;
	      else
		thisWay = 2;
	      break;
	    }
	  }
	  way[iColumn] = thisWay;
	}
	/*if (!numberBad)
	  printf("Was dual feasible before passes - rows %d\n",
	  numberRows_);*/
	int lastNumberIn = -100000;
	int numberPasses=5;
	while (numberIn>lastNumberIn+numberRows_/100) {
	  lastNumberIn = numberIn;
	  // we need to maximize chance of doing good
	  int iRow;
	  for (iRow=0;iRow<numberRows_;iRow++) {
	    double lowerBound = rowLower_[iRow];
	    double upperBound = rowUpper_[iRow];
	    if (getRowStatus(iRow)==basic) {
	      // see if we can find a column to pivot on
	      int j;
	      // down is amount pi can go down
	      double maximumDown = COIN_DBL_MAX;
	      double maximumUp = COIN_DBL_MAX;
	      double minimumDown =0.0;
	      double minimumUp =0.0;
	      int iUp=-1;
	      int iDown=-1;
	      int iUpB=-1;
	      int iDownB=-1;
	      if (lowerBound<-1.0e20)
		maximumUp = -1.0;
	      if (upperBound>1.0e20)
		maximumDown = -1.0;
	      for (j=rowStart[iRow];j<rowStart[iRow]+rowLength[iRow];j++) {
		int iColumn = column[j];
		double value = elementByRow[j];
		double djValue = dj[iColumn];
		/* way -
		   -3 - okay at upper bound with negative dj
		   -2 - marginal at upper bound with zero dj - can only decrease
		   -1 - bad at upper bound
		   0 - we can never pivot on this row
		   1 - bad at lower bound
		   2 - marginal at lower bound with zero dj - can only increase
		   3 - okay at lower bound with positive dj
		   100 - fine we can just ignore
		*/
		if (way[iColumn]!=100) {
		  switch(way[iColumn]) {
		    
		  case -3:
		    if (value>0.0) {
		      if (maximumDown*value>-djValue) {
			maximumDown = -djValue/value;
			iDown = iColumn;
		      }
		    } else {
		      if (-maximumUp*value>-djValue) {
			maximumUp = djValue/value;
			iUp = iColumn;
		      }
		    }
		    break;
		  case -2:
		    if (value>0.0) {
		      maximumDown = 0.0;
		    } else {
		      maximumUp = 0.0;
		    }
		    break;
		  case -1:
		    // see if could be satisfied
		    // dj value > 0
		    if (value>0.0) {
		      maximumDown=0.0;
		      if (maximumUp*value<djValue-dualTolerance) {
			maximumUp = 0.0; // would improve but not enough
		      } else {
			if (minimumUp*value<djValue) {
			  minimumUp = djValue/value;
			  iUpB = iColumn;
			}
		      }
		    } else {
		      maximumUp=0.0;
		      if (-maximumDown*value<djValue-dualTolerance) {
			maximumDown = 0.0; // would improve but not enough
		      } else {
			if (-minimumDown*value<djValue) {
			  minimumDown = -djValue/value;
			  iDownB = iColumn;
			}
		      }
		    }
		    
		    break;
		  case 0:
		    maximumDown = -1.0;
		    maximumUp=-1.0;
		    break;
		  case 1:
		    // see if could be satisfied
		    // dj value < 0
		    if (value>0.0) {
		      maximumUp=0.0;
		      if (maximumDown*value<-djValue-dualTolerance) {
			maximumDown = 0.0; // would improve but not enough
		      } else {
			if (minimumDown*value<-djValue) {
			  minimumDown = -djValue/value;
			  iDownB = iColumn;
			}
		      }
		    } else {
		      maximumDown=0.0;
		      if (-maximumUp*value<-djValue-dualTolerance) {
			maximumUp = 0.0; // would improve but not enough
		      } else {
			if (-minimumUp*value<-djValue) {
			  minimumUp = djValue/value;
			  iUpB = iColumn;
			}
		      }
		    }
		    
		    break;
		  case 2:
		    if (value>0.0) {
		      maximumUp = 0.0;
		    } else {
		      maximumDown = 0.0;
		    }
		    
		    break;
		  case 3:
		    if (value>0.0) {
		      if (maximumUp*value>djValue) {
			maximumUp = djValue/value;
			iUp = iColumn;
		      }
		    } else {
		      if (-maximumDown*value>djValue) {
			maximumDown = -djValue/value;
			iDown = iColumn;
		      }
		    }
		    
		    break;
		  default:
		    break;
		  }
		}
	      }
	      if (iUpB>=0)
		iUp=iUpB;
	      if (maximumUp<=dualTolerance||maximumUp<minimumUp)
		iUp=-1;
	      if (iDownB>=0)
		iDown=iDownB;
	      if (maximumDown<=dualTolerance||maximumDown<minimumDown)
		iDown=-1;
	      if (iUp>=0||iDown>=0) {
		// do something
		if (iUp>=0&&iDown>=0) {
		  if (maximumDown>maximumUp)
		    iUp=-1;
		}
		double change;
		int kColumn;
		if (iUp>=0) {
		  kColumn=iUp;
		  change=maximumUp;
		  // just do minimum if was dual infeasible
		  // ? only if maximum large?
		  if (minimumUp>0.0)
		    change=minimumUp;
		  setRowStatus(iRow,atUpperBound);
		} else {
		  kColumn=iDown;
		  change=-maximumDown;
		  // just do minimum if was dual infeasible
		  // ? only if maximum large?
		  if (minimumDown>0.0)
		    change=-minimumDown;
		  setRowStatus(iRow,atLowerBound);
		}
		assert (fabs(change)<1.0e20);
		setColumnStatus(kColumn,basic);
		numberIn++;
		pi[iRow]=change;
		for (j=rowStart[iRow];j<rowStart[iRow]+rowLength[iRow];j++) {
		  int iColumn = column[j];
		  double value = elementByRow[j];
		  double djValue = dj[iColumn]-change*value;
		  dj[iColumn]=djValue;
		  if (abs(way[iColumn])==1) {
		    numberBad--;
		    /*if (!numberBad)
		      printf("Became dual feasible at row %d out of %d\n",
		      iRow, numberRows_);*/
		    lastNumberIn=-1000000;
		  }
		  int thisWay = 100;
		  double lowerBound = columnLower_[iColumn];
		  double upperBound = columnUpper_[iColumn];
		  if (upperBound>lowerBound) {
		    switch(getColumnStatus(iColumn)) {
		      
		    case basic:
		      thisWay=0;
		    case isFixed:
		      break;
		    case isFree:
		    case superBasic:
		      if (djValue<-dualTolerance) 
			thisWay = 1;
		      else if (djValue>dualTolerance) 
			thisWay = -1;
		      else
			{ thisWay =0; abort();}
		      break;
		    case atUpperBound:
		      if (djValue>dualTolerance) 
			{ thisWay =-1; abort();}
		      else if (djValue<-dualTolerance) 
			thisWay = -3;
		      else
			thisWay = -2;
		      break;
		    case atLowerBound:
		      if (djValue<-dualTolerance) 
			{ thisWay =1; abort();}
		      else if (djValue>dualTolerance) 
			thisWay = 3;
		      else
			thisWay = 2;
		      break;
		    }
		  }
		  way[iColumn] = thisWay;
		}
	      }
	    }
	  }
	  if (numberIn==lastNumberIn||numberBad||pivot<2)
	    break;
	  if (!(--numberPasses))
	    break;
	  //printf("%d put in so far\n",numberIn);
	}
	// last attempt to flip
	for (iColumn=0;iColumn<numberColumns_;iColumn++) {
	  double lowerBound = columnLower_[iColumn];
	  double upperBound = columnUpper_[iColumn];
	  if (upperBound-lowerBound<=gap&&upperBound>lowerBound) {
	    double djValue=dj[iColumn];
	    switch(getColumnStatus(iColumn)) {
	      
	    case basic:
	    case ClpSimplex::isFixed:
	      break;
	    case isFree:
	    case superBasic:
	      break;
	    case atUpperBound:
	      if (djValue>dualTolerance) {
		setColumnStatus(iColumn,atUpperBound);
		solution[iColumn]=upperBound;
	      } 
	      break;
	    case atLowerBound:
	      if (djValue<-dualTolerance) {
		setColumnStatus(iColumn,atUpperBound);
		solution[iColumn]=upperBound;
	      }
	      break;
	    }
	  }
	}
	delete [] pi;
	delete [] dj;
	delete [] way;
	handler_->message(CLP_CRASH,messages_)
	  <<numberIn
	  <<numberBad
	  <<CoinMessageEol;
	return -1;
      }
    } else {
      delete [] dj;
      return -1;
    }
  }
}
int
ClpSimplex::nextSuperBasic()
{
  if (firstFree_>=0) {
    int returnValue=firstFree_;
    int iColumn=firstFree_+1;
    if (algorithm_>0) {
      // primal
      for (;iColumn<numberRows_+numberColumns_;iColumn++) {
	if (getStatus(iColumn)==superBasic) {
	  if (fabs(solution_[iColumn]-lower_[iColumn])<=primalTolerance_) {
	    solution_[iColumn]=lower_[iColumn];
	    setStatus(iColumn,atLowerBound);
	  } else if (fabs(solution_[iColumn]-upper_[iColumn])
		     <=primalTolerance_) {
	    solution_[iColumn]=upper_[iColumn];
	    setStatus(iColumn,atUpperBound);
	  } else if (lower_[iColumn]<-1.0e20&&upper_[iColumn]>1.0e20) {
	    setStatus(iColumn,isFree);
	    break;
	  } else {
	    break;
	  }
	}
      }
    } else {
      // dual
      for (;iColumn<numberRows_+numberColumns_;iColumn++) {
	if (getStatus(iColumn)==isFree) 
	  if (fabs(dj_[iColumn])>1.0e2*dualTolerance_) 
	    break;
      }
    }
    firstFree_ = iColumn;
    if (firstFree_==numberRows_+numberColumns_)
      firstFree_=-1;
    return returnValue;
  } else {
    return -1;
  }
}
/* Pivot in a variable and out a variable.  Returns 0 if okay,
   1 if inaccuracy forced re-factorization, -1 if would be singular.
   Also updates primal/dual infeasibilities.
   Assumes sequenceIn_ and pivotRow_ set and also directionIn and Out.
*/
int ClpSimplex::pivot()
{
  // scaling not allowed
  assert (!scalingFlag_);
  // assume In_ and Out_ are correct and directionOut_ set
  // (or In_ if flip
  lowerIn_ = lower_[sequenceIn_];
  valueIn_ = solution_[sequenceIn_];
  upperIn_ = upper_[sequenceIn_];
  dualIn_ = dj_[sequenceIn_];
  if (sequenceOut_>=0&&sequenceIn_!=sequenceIn_) {
    assert (pivotRow_>=0&&pivotRow_<numberRows_);
    assert (pivotVariable_[pivotRow_]==sequenceOut_);
    lowerOut_ = lower_[sequenceOut_];
    valueOut_ = solution_[sequenceOut_];
    upperOut_ = upper_[sequenceOut_];
    // for now assume primal is feasible (or in dual)
    dualOut_ = dj_[sequenceOut_];
    assert(fabs(dualOut_)<1.0e-6);
  } else {
    assert (pivotRow_<0);
  }
  bool roundAgain = true;
  int returnCode=0;
  while (roundAgain) {
    roundAgain=false;
    unpack(rowArray_[1]);
    factorization_->updateColumn(rowArray_[2],rowArray_[1],true);
    // we are going to subtract movement from current basic
    double movement;
    // see where incoming will go to
    if (sequenceOut_<0||sequenceIn_==sequenceOut_) {
      // flip so go to bound
      movement  = ((directionIn_>0) ? upperIn_ : lowerIn_) - valueIn_;
    } else {
      // get where outgoing needs to get to
      double outValue = (directionOut_>0) ? upperOut_ : lowerOut_;
      // solutionOut_ - movement*alpha_ == outValue
      movement = (outValue-valueOut_)/alpha_;
      // set directionIn_ correctly
      directionIn_ = (movement>0) ? 1 :-1;
    }
    // update primal solution
    {
      int i;
      int * index = rowArray_[1]->getIndices();
      int number = rowArray_[1]->getNumElements();
      double * element = rowArray_[1]->denseVector();
      for (i=0;i<number;i++) {
	int ii = index[i];
	// get column
	ii = pivotVariable_[ii];
	solution_[ii] -= movement*element[i];
	element[i]=0.0;
      }
      // see where something went to
      if (sequenceOut_<0) {
	if (directionIn_<0) {
	  assert (fabs(solution_[sequenceIn_]-upperIn_)<1.0e-7);
	  solution_[sequenceIn_]=upperIn_;
	} else {
	  assert (fabs(solution_[sequenceIn_]-lowerIn_)<1.0e-7);
	  solution_[sequenceIn_]=lowerIn_;
	}
      } else {
	if (directionOut_<0) {
	  assert (fabs(solution_[sequenceOut_]-upperOut_)<1.0e-7);
	  solution_[sequenceOut_]=upperOut_;
	} else {
	  assert (fabs(solution_[sequenceOut_]-lowerOut_)<1.0e-7);
	  solution_[sequenceOut_]=lowerOut_;
	}
	solution_[sequenceIn_]=valueIn_+movement;
      }
    }    
    double objectiveChange = dualIn_*movement;
    // update duals
    if (pivotRow_>=0) {
      alpha_ = rowArray_[1]->denseVector()[pivotRow_];
      assert (fabs(alpha_)>1.0e-8);
      double multiplier = dualIn_/alpha_;
      rowArray_[0]->insert(pivotRow_,multiplier);
      factorization_->updateColumnTranspose(rowArray_[2],rowArray_[0]);
      // put row of tableau in rowArray[0] and columnArray[0]
      matrix_->transposeTimes(this,-1.0,
			      rowArray_[0],columnArray_[1],columnArray_[0]);
      // update column djs
      int i;
      int * index = columnArray_[0]->getIndices();
      int number = columnArray_[0]->getNumElements();
      double * element = columnArray_[0]->denseVector();
      for (i=0;i<number;i++) {
	int ii = index[i];
	dj_[ii] += element[ii];
	element[ii]=0.0;
      }
      columnArray_[0]->setNumElements(0);
      // and row djs
      index = rowArray_[0]->getIndices();
      number = rowArray_[0]->getNumElements();
      element = rowArray_[0]->denseVector();
      for (i=0;i<number;i++) {
	int ii = index[i];
	dj_[ii+numberColumns_] += element[ii];
	dual_[ii] = dj_[ii+numberColumns_];
	element[ii]=0.0;
      }
      rowArray_[0]->setNumElements(0);
      // check incoming
      assert (fabs(dj_[sequenceIn_])<1.0e-6);
    }
    
    // if stable replace in basis
    int updateStatus = factorization_->replaceColumn(rowArray_[2],
						   pivotRow_,
						     alpha_);
    bool takePivot=true;
    // if no pivots, bad update but reasonable alpha - take and invert
    if (updateStatus==2&&
	lastGoodIteration_==numberIterations_&&fabs(alpha_)>1.0e-5)
      updateStatus=4;
    if (updateStatus==1||updateStatus==4) {
      // slight error
      if (factorization_->pivots()>5||updateStatus==4) {
	returnCode=-1;
      }
    } else if (updateStatus==2) {
      // major error
      rowArray_[1]->clear();
      takePivot=false;
      if (factorization_->pivots()) {
	// refactorize here
	statusOfProblem();
	roundAgain=true;
      } else {
	returnCode=1;
      }
    } else if (updateStatus==3) {
      // out of memory
      // increase space if not many iterations
      if (factorization_->pivots()<
	  0.5*factorization_->maximumPivots()&&
	  factorization_->pivots()<200)
	factorization_->areaFactor(
				   factorization_->areaFactor() * 1.1);
      returnCode =-1; // factorize now
    }
    if (takePivot) {
      int save = algorithm_;
      // make simple so always primal
      algorithm_=1;
      housekeeping(objectiveChange);
      algorithm_=save;
    }
  }
  if (returnCode == -1) {
    // refactorize here
    statusOfProblem();
  } else {
    // just for now - recompute anyway
    gutsOfSolution(rowActivityWork_,columnActivityWork_,NULL,NULL);
  }
  return returnCode;
}

/* Pivot in a variable and choose an outgoing one.  Assumes primal
   feasible - will not go through a bound.  Returns step length in theta
   Returns ray in ray_ (or NULL if no pivot)
   Return codes as before but -1 means no acceptable pivot
*/
int ClpSimplex::primalPivotResult()
{
  assert (sequenceIn_>=0);
  valueIn_=solution_[sequenceIn_];
  lowerIn_=lower_[sequenceIn_];
  upperIn_=upper_[sequenceIn_];
  dualIn_=dj_[sequenceIn_];

  int returnCode = ((ClpSimplexPrimal *) this)->pivotResult();
  if (returnCode<0&&returnCode>-4) {
    return 0;
  } else {
    printf("Return code of %d from ClpSimplexPrimal::pivotResult\n",
	   returnCode);
    return -1;
  }
}
  
/* Pivot out a variable and choose an incoing one.  Assumes dual
   feasible - will not go through a reduced cost.  
   Returns step length in theta
   Returns ray in ray_ (or NULL if no pivot)
   Return codes as before but -1 means no acceptable pivot
*/
int 
ClpSimplex::dualPivotResult()
{
  return ((ClpSimplexDual *) this)->pivotResult();
}
// Common bits of coding for dual and primal
int 
ClpSimplex::startup(int ifValuesPass)
{
  // sanity check
  assert (numberRows_==matrix_->getNumRows());
  assert (numberColumns_==matrix_->getNumCols());
  // for moment all arrays must exist
  assert(columnLower_);
  assert(columnUpper_);
  assert(rowLower_);
  assert(rowUpper_);


  primalTolerance_=dblParam_[ClpPrimalTolerance];
  dualTolerance_=dblParam_[ClpDualTolerance];

  // put in standard form (and make row copy)
  // create modifiable copies of model rim and do optional scaling
  bool goodMatrix=createRim(7+8+16,true);

  if (goodMatrix) {
    // Model looks okay
    // Do initial factorization
    // and set certain stuff
    // We can either set increasing rows so ...IsBasic gives pivot row
    // or we can just increment iBasic one by one
    // for now let ...iBasic give pivot row
    factorization_->increasingRows(2);
    // row activities have negative sign
    factorization_->slackValue(-1.0);
    factorization_->zeroTolerance(1.0e-13);
    // Switch off dense
    int saveThreshold = factorization_->denseThreshold();
    factorization_->setDenseThreshold(0);
    numberIterations_=0;

    // do perturbation if asked for

    if (perturbation_<100) {
      if (algorithm_>0) {
	((ClpSimplexPrimal *) this)->perturb();
      } else if (algorithm_<0) {
	((ClpSimplexDual *) this)->perturb();
      }
    }
    // for primal we will change bounds using infeasibilityCost_
    if (nonLinearCost_==NULL&&algorithm_>0) {
      // get a valid nonlinear cost function
      delete nonLinearCost_;
      nonLinearCost_= new ClpNonLinearCost(this);
    }
    
    // loop round to clean up solution if values pass
    int numberThrownOut = -1;
    int totalNumberThrownOut=0;
    while(numberThrownOut) {
      int status = internalFactorize(0+10*ifValuesPass);
      if (status<0)
	return 1; // some error
      else
	totalNumberThrownOut+= status;
      
      // for this we need clean basis so it is after factorize
      numberThrownOut=gutsOfSolution(rowActivityWork_,columnActivityWork_,
				     NULL,NULL,
				     ifValuesPass!=0);
      totalNumberThrownOut+= numberThrownOut;
      
    }
    
    if (totalNumberThrownOut)
      handler_->message(CLP_SINGULARITIES,messages_)
	<<totalNumberThrownOut
	<<CoinMessageEol;
    // Switch back dense
    factorization_->setDenseThreshold(saveThreshold);
    
    problemStatus_ = -1;
    
    // number of times we have declared optimality
    numberTimesOptimal_=0;

    return 0;
  } else {
    // bad matrix
    return 2;
  }
    
}


void 
ClpSimplex::finish()
{
  // Get rid of some arrays and empty factorization
  deleteRim();
  assert(problemStatus_>=0&&problemStatus_<5);
  handler_->message(CLP_SIMPLEX_FINISHED+problemStatus_,messages_)
    <<objectiveValue()
    <<CoinMessageEol;
  factorization_->relaxAccuracyCheck(1.0);
  // get rid of any network stuff - could do more
  factorization_->cleanUp();
}
// Save data
ClpDataSave 
ClpSimplex::saveData() const
{
  ClpDataSave saved;
  saved.dualBound_ = dualBound_;
  saved.infeasibilityCost_ = infeasibilityCost_;
  saved.sparseThreshold_ = factorization_->sparseThreshold();
  saved.perturbation_ = perturbation_;
  return saved;
}
// Restore data
void 
ClpSimplex::restoreData(ClpDataSave saved)
{
  factorization_->sparseThreshold(saved.sparseThreshold_);
  perturbation_ = saved.perturbation_;
  infeasibilityCost_ = saved.infeasibilityCost_;
  dualBound_ = saved.dualBound_;
}
/* Factorizes and returns true if optimal.  Used by user */
bool
ClpSimplex::statusOfProblem()
{
  // is factorization okay?
  assert (internalFactorize(1)==0);
  // put back original costs and then check
  // also move to work arrays
  createRim(4+32);
  abort();
  //memcpy(rowActivityWork_,rowActivity_,numberRows_*sizeof(double));
  //memcpy(columnActivityWork_,columnActivity_,numberColumns_*sizeof(double));
  gutsOfSolution(rowActivityWork_, columnActivityWork_,NULL,NULL);
  //memcpy(rowActivity_,rowActivityWork_,numberRows_*sizeof(double));
  //memcpy(columnActivity_,columnActivityWork_,numberColumns_*sizeof(double));
  //memcpy(reducedCost_,dj_,numberColumns_*sizeof(double));
  deleteRim(-1);
  return (primalFeasible()&&dualFeasible());
}
/* Return model - updates any scalars */
void 
ClpSimplex::returnModel(ClpSimplex & otherModel)
{
  ClpModel::returnModel(otherModel);
  otherModel.columnPrimalInfeasibility_ = columnPrimalInfeasibility_;
  otherModel.columnPrimalSequence_ = columnPrimalSequence_;
  otherModel.rowPrimalInfeasibility_ = rowPrimalInfeasibility_;
  otherModel.rowPrimalSequence_ = rowPrimalSequence_;
  otherModel.columnDualInfeasibility_ = columnDualInfeasibility_;
  otherModel.columnDualSequence_ = columnDualSequence_;
  otherModel.rowDualInfeasibility_ = rowDualInfeasibility_;
  otherModel.rowDualSequence_ = rowDualSequence_;
  otherModel.primalToleranceToGetOptimal_ = primalToleranceToGetOptimal_;
  otherModel.remainingDualInfeasibility_ = remainingDualInfeasibility_;
  otherModel.largestPrimalError_ = largestPrimalError_;
  otherModel.largestDualError_ = largestDualError_;
  otherModel.largestSolutionError_ = largestSolutionError_;
  otherModel.alpha_ = alpha_;
  otherModel.theta_ = theta_;
  otherModel.lowerIn_ = lowerIn_;
  otherModel.valueIn_ = valueIn_;
  otherModel.upperIn_ = upperIn_;
  otherModel.dualIn_ = dualIn_;
  otherModel.sequenceIn_ = sequenceIn_;
  otherModel.directionIn_ = directionIn_;
  otherModel.lowerOut_ = lowerOut_;
  otherModel.valueOut_ = valueOut_;
  otherModel.upperOut_ = upperOut_;
  otherModel.dualOut_ = dualOut_;
  otherModel.sequenceOut_ = sequenceOut_;
  otherModel.directionOut_ = directionOut_;
  otherModel.pivotRow_ = pivotRow_;
  otherModel.sumDualInfeasibilities_ = sumDualInfeasibilities_;
  otherModel.numberDualInfeasibilities_ = numberDualInfeasibilities_;
  otherModel.numberDualInfeasibilitiesWithoutFree_ = 
    numberDualInfeasibilitiesWithoutFree_;
  otherModel.sumPrimalInfeasibilities_ = sumPrimalInfeasibilities_;
  otherModel.numberPrimalInfeasibilities_ = numberPrimalInfeasibilities_;
  otherModel.numberTimesOptimal_ = numberTimesOptimal_;
  otherModel.sumOfRelaxedDualInfeasibilities_ = sumOfRelaxedDualInfeasibilities_;
  otherModel.sumOfRelaxedPrimalInfeasibilities_ = sumOfRelaxedPrimalInfeasibilities_;
}
/* Constructs a non linear cost from list of non-linearities (columns only)
   First lower of each column is taken as real lower
   Last lower is taken as real upper and cost ignored
   
   Returns nonzero if bad data e.g. lowers not monotonic
*/
int 
ClpSimplex::createPiecewiseLinearCosts(const int * starts,
				       const double * lower, const double * gradient)
{
  delete nonLinearCost_;
  // Set up feasible bounds and check monotonicity
  int iColumn;
  int returnCode=0;

  for (iColumn=0;iColumn<numberColumns_;iColumn++) {
    int iIndex = starts[iColumn];
    int end = starts[iColumn+1]-1;
    columnLower_[iColumn] = lower[iIndex];
    columnUpper_[iColumn] = lower[end];
    double value = columnLower_[iColumn];
    iIndex++;
    for (;iIndex<end;iIndex++) {
      if (lower[iIndex]<value)
	returnCode++; // not monotonic
      value=lower[iIndex];
    }
  }
  nonLinearCost_ = new ClpNonLinearCost(this,starts,lower,gradient);
  specialOptions_ |= 2; // say keep
  return returnCode;
}
