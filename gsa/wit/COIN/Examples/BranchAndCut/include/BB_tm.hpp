// Copyright (C) 2001, International Business Machines
// Corporation and others.  All Rights Reserved.
#ifndef _BB_TM_H
#define _BB_TM_H

// This file is fully docified.

#include "BCP_tm_user.hpp"
#include "BB.hpp"

#include "BCP_parameters.hpp"
#include "BB_tm_param.hpp"
#include "BB_lp_param.hpp"

//#############################################################################

/**
   The BCP_tm_user class is the base class from which the user
   can derive a problem specific class to be used in the TM process.

   In that derived class the user can store data to be used in the methods she
   overrides. Also that is the object the user must return in the
   USER_initialize::tm_init() method.
   
   There are two kind of methods in the class. The non-virtual methods are
   helper functions for the built-in defaults, but the user can use them as
   well. The virtual methods execute steps in the BCP algorithm where the user
   might want to override the default behavior.

   The default implementations fall into three major categories. 
   <ul>
     <li> Empty; doesn't do anything and immediately returns. (E.g., 
          pack_module_data().
     <li> There is no reasonable default, so throw an exception. This happens
          if the parameter settings drive the flow of in a way that BCP can't
	  perform the necessary function. This behavior is correct since such
	  methods are invoked only if the parameter settings drive the flow of
	  the algorithm that way, in which case the user better implement those
	  methods. (At the momemnt there is no such method in TM.)
     <li> A default is given. Frequently there are multiple defaults and
          parameters govern which one is selected (e.g.,
	  compare_tree_nodes()).
   </ul>
*/

class BB_tm : public BCP_tm_user {
private:
   BCP_parameter_set<BB_tm_par> tm_par;
   BCP_parameter_set<BB_lp_par> lp_par;
   BB_prob desc;
public:
  //===========================================================================
  /**@name Constructor, Destructor */
  /*@{*/
    BB_tm() {}
    /** Being virtual, the destructor invokes the destructor for the real type
	of the object being deleted. */
    virtual ~BB_tm() {}
  /*@}*/

   void readInput(const char* filename);

  //===========================================================================
  // Here are the user defined functions. For each of them a default is given
  // which can be overridden when the concrete user class is defined.
  //===========================================================================

  /**@name Packing and unpacking methods */

  /*@{*/
    /** Pack the initial information (info that the user wants to send over)
	for the process specified by the last argument. The information packed
	here will be unpacked in the <code>unpack_module_data()</code> method
	of the user defined class in the appropriate process. <br>
	Default: empty method.
    */
    virtual void
    pack_module_data(BCP_buffer& buf, BCP_process_t ptype);

    /** Unpack a MIP feasible solution that was packed by the
	BCP_lp_user::pack_feasible_solution() method.

	Default: Unpacks a BCP_solution_generic object. The built-in default
	should be used if and only if the built-in default was used
	in BCP_lp_user::pack_feasible_solution().
    */
//      virtual BCP_solution*
//      unpack_feasible_solution(BCP_buffer& buf);

    /**@name Methods that pack/unpack warmstart, var_algo and cut_algo objects.

       The packing methods take an object and a buffer as
       an argument and the user is supposed to pack the object into the buffer.

       The argument of the unpacking methods is just the buffer. The user
       is supposed to return a pointer to the unpacked object.
    */
    /*@{*/
      /** Pack warmstarting information */
//        virtual void
//        pack_warmstart(const BCP_warmstart* ws, BCP_buffer& buf);
      /** Unpack warmstarting information */
//        virtual BCP_warmstart*
//        unpack_warmstart(BCP_buffer& buf);
      
      /** Pack an algorithmic variable */
//        virtual void
//        pack_var_algo(const BCP_var_algo* var, BCP_buffer& buf);
      /** Unpack an algorithmic variable */
//        virtual BCP_var_algo*
//        unpack_var_algo(BCP_buffer& buf);
      
      /** Pack an algorithmic cut */
      virtual void
      pack_cut_algo(const BCP_cut_algo* cut, BCP_buffer& buf);
      /** Unpack an algorithmic cut */
      virtual BCP_cut_algo*
      unpack_cut_algo(BCP_buffer& buf);
    /*@}*/
  /*@}*/

  //--------------------------------------------------------------------------
  /**@name Initial setup (creating core and root) */
  /*@{*/
    /** Create the core of the problem by filling out the last three arguments.
	These variables/cuts will always stay in the LP relaxation and the
	corresponding matrix is described by the specified matrix. If there is
	no core variable or cut then the returned pointer for to the matrix
	should be a null pointer.

	Default: empty method, meaning that there are no variables/cuts in the
	core and this the core matrix is empty (0 pointer) as well.
    */
     virtual void
     initialize_core(BCP_vec<BCP_var_core*>& vars,
		     BCP_vec<BCP_cut_core*>& cuts,
		     BCP_lp_relax*& matrix);
    //-------------------------------------------------------------------------
    /** Create the set of extra variables and cuts that should be added to the
        formulation in the root node. Also decide how variable pricing shuld be
        done, that is, if column generation is requested in the
        init_new_phase() method of this class then column
        generation should be performed according to \c pricing_status.

        Default: empty method, meaning that no variables/cuts are added and no
        pricing should be done.
    */
//       virtual void
//       create_root(BCP_vec<BCP_var*>& added_vars,
//  		 BCP_vec<BCP_cut*>& added_cuts,
//  		 BCP_pricing_status& pricing_status);
  /*@}*/

  //--------------------------------------------------------------------------
  /** Display a feasible solution */
  virtual void
  display_feasible_solution(const BCP_solution* sol);
    
  //---------------------------------------------------------------------------
  /**@name Initialize new phase */
  /*@{*/
    /** Do whatever initialization is necessary before the
        <code>phase</code>-th phase. (E.g., setting the pricing strategy.) */
//      virtual void
//      init_new_phase(int phase, BCP_column_generation& colgen);
  /*@}*/

  //---------------------------------------------------------------------------
  /**@name Search tree node comparison */
  /*@{*/
    /**@name Compare two search tree nodes. Return true if the first node
       should be processed before the second one.

       Default: The default behavior is controlled by the
       \c TreeSearchStrategy  parameter which is set to
       0 (\c BCP_BestFirstSearch) by default.
    */
//      virtual bool compare_tree_nodes(const BCP_tm_node* node0,
//  				    const BCP_tm_node* node1);
  /*@}*/
};

//#############################################################################

#endif
