C Copyright (C) 2002, Carnegie Mellon University and others.
C All Rights Reserved.
C This code is published under the Common Public License.
C*******************************************************************************
C
      subroutine GET_LAMBDA(N, NIND, M, ITER, IVAR, NFIX, IFIX,
     1     NORIG, XORIG, CSCALE, G, NLB, ILB, NUB, IUB, V_L, V_U,
     2     LSLACKS, LAM,
     1     KCONSTR, LRS, RS, LIS, IS, LRW, RW, LIW, IW, IERR)
C
C*******************************************************************************
C
C    $Id: get_lambda.f,v 1.1 2002/05/02 18:52:17 andreasw Exp $
C
C-------------------------------------------------------------------------------
C                                 Title
C-------------------------------------------------------------------------------
C
CT    Compute lambda (coordinate multipliers)
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
CP   G         I    INT    gradient of objective function
CP   NLB       I    INT    number of lower bounds (excluding fixed vars)
CP   ILB       I    INT    indices of lower bounds
CP                            (e.g. V_L(i) is bound for X(ILB(i)) )
CP   NUB       I    INT    number of upper bounds (excluding fixed vars)
CP   IUB       I    INT    indices of upper bounds
CP                            (e.g. V_U(i) is bound for X(IUB(i)) )
CP   V_L       I    DP     dual variables for lower bounds
CP   V_U       I    DP     dual variables for upper bounds
CP   LSLACKS   I    LOG    =.true.: There are slacks that don't satisfy
CP                                  "slack equation"
CP   LAM       O    DP     multipliers
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
CS    CONSTR
CS    DSCAL
CS    DCOPY
CS    C_OUT
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
      double precision G(N)
      integer NLB
      integer ILB(NLB)
      integer NUB
      integer IUB(NUB)
      double precision V_L(NLB)
      double precision V_U(NUB)
      logical LSLACKS
      double precision LAM(M+N) ! larger for LSLACKS
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
      integer p_iwend, p_rwend, p_rhs
      integer idummy, i, k
      character*80 line
C
C*******************************************************************************
C
C                           Executable Statements
C
C*******************************************************************************
C
      p_iwend = 0
      p_rwend = 0
      IERR = 0

      if( M.eq.0 ) goto 9999   ! Nothing to do if no constraints
C
      if( QLAMBDA.eq.1 ) then

         p_rhs   = p_rwend
         p_rwend = p_rhs + M
         if( p_rwend.gt.LRW ) then
            IERR = 98
            goto 9999
         endif
C
C     Compute rhs
C
         call DCOPY(M, G, 1, RW(p_rhs+1) ,1)
C
C     Use the dual variables instead of the real gradient of the log barrier
C     term, since the latter changes too drastically when MU is updated.
C
         do i = 1, NLB
            if( ILB(i).le.M ) then
               k = ILB(i)
               RW(p_rhs+k) = RW(p_rhs+k) - V_L(i)
            endif
         enddo
         do i = 1, NUB
            if( IUB(i).le.M ) then
               k = IUB(i)
               RW(p_rhs+k) = RW(p_rhs+k) + V_U(i)
            endif
         enddo
C
C     Call CONSTR
C
         call CONSTR(3, ITER, N, NIND, M, IVAR, NFIX, IFIX,
     1        NORIG, XORIG, CSCALE, RW(p_rhs+1), LAM, 0, idummy,
     3        KCONSTR(1), RS(KCONSTR(2)+1), KCONSTR(3),
     4        IS(KCONSTR(4)+1), LRW-p_rwend, RW(p_rwend+1),
     5        LIW-p_iwend, IW(p_iwend+1), IERR)
         if( IERR.lt.0 ) then
            write(line,*) 'get_rg: Warning in CONSTR, IERR = ',IERR
            call C_OUT(2,0,1,line)
         elseif( IERR.ne.0 ) then
            write(line,*) 'get_rg: Error in CONSTR, IERR = ',IERR
            call C_OUT(2,0,1,line)
            goto 9999
         endif
C
C     Correct sign
C
         call DSCAL(M, -1.d0, LAM, 1)
      else
         write(line,*) 'get_lambda: Invalid QLAMBDA!'
         IERR = 4
         goto 9999
      endif
C
C     If we have additional slack equations and use augmented Lagrangian
C     compute those multipliers explicitly
C
      if( LSLACKS .and. QMERIT.eq.3 ) then
         call DCOPY(N, 0d0, 0, LAM(M+1), 1)
         do i = 1, NLB
            k = ILB(i)
            LAM(M+k) = -V_L(i)
         enddo
         do i = 1, NUB
            k = IUB(i)
            LAM(M+k) = LAM(M+k) + V_U(i)
         enddo
      endif
C
C     That's it
C
 9999 continue
      return
      end
