C Copyright (C) 2002, Carnegie Mellon University and others.
C All Rights Reserved.
C This code is published under the Common Public License.
C*******************************************************************************
C
      subroutine GET_BB(N, NIND, M, ITER, IVAR, NFIX, IFIX,
     1                  NORIG, XORIG, CSCALE,
     1                  NLB, ILB, NUB, IUB, SIGMA_L, SIGMA_U, S_L, S_U,
     1                  BB, RESTO, KCONSTR, LRS, RS, LIS, IS,
     2                  LRW, RW, LIW, IW, IERR)
C
C*******************************************************************************
C
C    $Id: get_bb.f,v 1.2 2002/07/24 03:07:54 andreasw Exp $
C
C-------------------------------------------------------------------------------
C                                 Title
C-------------------------------------------------------------------------------
C
CT    Compute reduced Hessian of analytic barrier function
C
C-------------------------------------------------------------------------------
C                          Programm description
C-------------------------------------------------------------------------------
C
CB    
C
C-------------------------------------------------------------------------------
C                             Author, date
C-------------------------------------------------------------------------------
C
CA    Andreas Waechter      05/01/02  Release as version IPOPT 2.0
C
C-------------------------------------------------------------------------------
C                             Documentation
C-------------------------------------------------------------------------------
C
CD
C
C-------------------------------------------------------------------------------
C                             Parameter list    
C-------------------------------------------------------------------------------
C
C    Name     I/O   Type   Meaning
C
CP   N         I    INT    number of variables
CP   NIND      I    INT    number of independent variables
CP   M         I    INT    number of equality constraints / dependent variables
CP   ITER      I    INT    iteration counter
CP   IVAR      I    INT    information about partitioning
CP                            i = 1..M      XORIG(IVAR(i)) dependent
CP                            i = (M+1)..N  XORIG(IVAR(i)) independent
CP                            Note: fixed variables do not occur in IVAR
CP   NFIX      I    INT    number of fixed variables
CP   IFIX      I    INT    specifies variables that are fixed by bounds:
CP                            i = 1..NORIG-N   XORIG(IFIX(i)) is fixed
CP   NORIG     I    INT    total number of all variables (incl. fixed vars)
CP   XORIG     I    DP     (only TASK = 1,2,3): actual iterate
CP                            XORIG is ordered in ORIGINAL order (i.e. not
CP                            partitioned into independent and dependent
CP                            variables)
CP   CSCALE    I    DP     scaling factors for constraints
CP   NLB       I    INT    number of lower bounds (excluding fixed vars)
CP   ILB       I    INT    indices of lower bounds
CP                            (e.g. S_L(i) is slack for X(ILB(i)) )
CP   NUB       I    INT    number of upper bounds (excluding fixed vars)
CP   IUB       I    INT    indices of upper bounds
CP                            (e.g. S_U(i) is slack for X(IUB(i)) )
CP   SIGMA_L   I    DP     primal-dual Hessian of lower bound barrier term
CP                            (NLB diagonal elements only)
CP   SIGMA_U   I    DP     primal-dual Hessian of upper bound barrier term
CP                            (NUB diagonal elements only)
CP   S_L       I    DP     slack variables for lower bounds
CP   S_U       I    DP     slack variables for upper bounds
CP   BB        O    DP     reduced primal-dual Hessian
CP   RESTO     I    LOG    =.true.: we are in restoration phase
CP   KCONSTR   I    INT    KCONSTR(1): LRS for CONSTR
CP                         KCONSTR(2): P_LRS for CONSTR
CP                         KCONSTR(3): LIS for CONSTR
CP                         KCONSTR(4): P_LIS for CONSTR
CP                         KCONSTR(5): LRW for CONSTR
CP                         KCONSTR(6): LIW for CONSTR
CP   LRS       I    INT    total length of RS
CP   RS       I/O   DP     DP storage space (all!)
CP   LIS       I    INT    total length of IS
CP   IS       I/O   INT    INT storage space (all!)
CP   LRW      I/O   INT    length of RW
CP   RW       I/O   DP     can be used as DP work space but content will be
CP                            changed between calls
CP   LIW      I/O   INT    length of IW
CP   IW       I/O   INT    can be used as INT work space but content will be
CP                            changed between calls
CP   IERR      O    INT    =0: everything OK
CP                         >0: Error occured; abort optimization
CP                         <0: Warning; message to user
C
C-------------------------------------------------------------------------------
C                             local variables
C-------------------------------------------------------------------------------
C
CL
C
C-------------------------------------------------------------------------------
C                             used subroutines
C-------------------------------------------------------------------------------
C
CS    DCOPY
CS    C_OUT
CS    CONSTR
CS    DASV2F
C
C*******************************************************************************
C
C                              Declarations
C
C*******************************************************************************
C
      IMPLICIT NONE
C
C*******************************************************************************
C
C                              Include files
C
C*******************************************************************************
C
      include 'IPOPT.INC'
      include 'TIMER.INC'
C
C-------------------------------------------------------------------------------
C                             Parameter list
C-------------------------------------------------------------------------------
C
      integer N
      integer NIND
      integer M
      integer ITER
      integer IVAR(N)
      integer NFIX
      integer IFIX(NFIX)
      integer NORIG
      double precision XORIG(NORIG)
      double precision CSCALE(*)
      integer NLB
      integer ILB(NLB)
      integer NUB
      integer IUB(NUB)
      double precision SIGMA_L(NLB)
      double precision SIGMA_U(NUB)
      double precision S_L(NLB)
      double precision S_U(NUB)
      double precision BB((NIND*(NIND+1))/2)
      logical RESTO
      integer KCONSTR(6)
      integer LRS
      double precision RS(LRS)
      integer LIS
      integer IS(LIS)
      integer LRW
      double precision RW(LRW)
      integer LIW
      integer IW(LIW)
      integer IERR
C
C-------------------------------------------------------------------------------
C                            Local varibales
C-------------------------------------------------------------------------------
C
      integer p_iwend, p_rwend, p_sigma
      integer idummy, i, k
      character*80 line
      double precision times, timef
C
C*******************************************************************************
C
C                           Executable Statements
C
C*******************************************************************************
C
      call TIMER(times)

      p_iwend = 0
      p_rwend = 0
      IERR = 0

      p_sigma = p_rwend
      p_rwend = p_sigma + N
      if( p_rwend.gt.LRW ) then
         IERR = 98
         goto 9999
      endif
C
C     Compute diagonal matrix
C
      if( .not.RESTO ) then
C
C     SIGMA + SIGMA to simga
C
         call DASV2F(N, NLB, ILB, NUB, IUB,
     1        SIGMA_L, SIGMA_U, RW(p_sigma+1))

      else
C
C     Compute scaling matrix
C
         call DCOPY(N, QDDMAX, 0, RW(p_sigma+1), 1)
         do i = 1, NLB
            k = ILB(i)
            RW(p_sigma+k) = dmax1(RW(p_sigma+k), 1d0/(S_L(i)**2))
         enddo
         do i = 1, NUB
            k = IUB(i)
            RW(p_sigma+k) = dmax1(RW(p_sigma+k), 1d0/(S_U(i)**2))
         enddo
      endif
C
      if( M.ne.0 ) then
C
C     Call CONSTR to get dependent part of BB
C
         call CONSTR(6, ITER, N, NIND, M, IVAR, NFIX, IFIX,
     1               NORIG, XORIG, CSCALE, RW(p_sigma+1), BB,
     1               idummy, idummy,
     3               KCONSTR(1), RS(KCONSTR(2)+1), KCONSTR(3),
     4               IS(KCONSTR(4)+1), LRW-p_rwend, RW(p_rwend+1),
     5               LIW-p_iwend, IW(p_iwend+1), IERR)
         if( IERR.lt.0 ) then
            write(line,*) 'get_bb: Warning in CONSTR, IERR = ',IERR
            call C_OUT(2,0,1,line)
            IERR = 0
         elseif( IERR.ne.0 ) then
            write(line,*) 'get_bb: Error in CONSTR, IERR = ',IERR
            call C_OUT(2,0,1,line)
            goto 9999
         endif
      else
         call DCOPY((NIND*(NIND+1))/2, 0.d0, 0, BB, 1)
      endif
C
C     Add independent part to BB
C
      k = 0
      do i = 1, NIND
         k = k + i
         BB(k) = BB(k) + RW(p_sigma+M+i)
      enddo
C
C     Free work space
C
      p_rwend = p_sigma
C
C     That's it
C
 9999 continue
      call TIMER(timef)
      TIME_BB = TIME_BB + timef - times
      return
      end
