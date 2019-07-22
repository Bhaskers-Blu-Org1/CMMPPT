C Copyright (C) 2002, Carnegie Mellon University and others.
C All Rights Reserved.
C This code is published under the Common Public License.
C*******************************************************************************
C
      subroutine GET_HV(ITER, N, NIND, NFIX, IFIX, X, IVAR, NORIG,
     1     XORIG, NLB, ILB, NUB, IUB, S_L, S_U, CSCALE, M, LAM,
     1     VIN, VOUT,
     2     KCONSTR, LRS, RS, LIS, IS, LRW, RW, LIW, IW, IERR)
C
C*******************************************************************************
C
C    $Id: get_hv.f,v 1.2 2002/07/24 03:12:12 andreasw Exp $
C
C-------------------------------------------------------------------------------
C                                 Title
C-------------------------------------------------------------------------------
C
CT    Evaluate product of Hessian of Lagrangian at X with vector
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
CP   ITER      I    INT    iteration counter
CP                         (If changed compared to last call, reevaluate
CP                          entries in Hessian and copy X to XORIG)
CP                         =-1: initialization
CP   N         I    INT    number of free variables
CP   NIND      I    INT    number of independent variables
CP   NFIX      I    INT    number of fixed variables
CP   IFIX      I    INT    specifies variables that are fixed by bounds:
CP                            i = 1..NORIG-N   XORIG(IFIX(i)) is fixed
CP   X         I    DP     values of free variables, where Hessian should be
CP                            evaluated
CP   IVAR      I    INT    information about partitioning
CP                            i = 1..M      XORIG(IVAR(i)) dependent
CP                            i = (M+1)..N  XORIG(IVAR(i)) independent
CP                            Note: fixed variables do not occur in IVAR
CP                              X(i) corresponds to XORIG(IVAR(i))
CP   NORIG     I    INT    number of all variables including fixed vars
CP   XORIG    I/O   DP     I: values of fixed variables
CP                         O: values of all variables (from X)
CP   NLB       I    INT    number of lower bounds (excluding fixed vars)
CP   ILB       I    INT    indices of lower bounds
CP                            (e.g. S_L(i) is slack for X(ILB(i)) )
CP   NUB       I    INT    number of upper bounds (excluding fixed vars)
CP   ILB       I    INT    indices of upper bounds
CP                            (e.g. S_U(i) is slack for X(IUB(i)) )
CP   S_L       I    DP     slack variables for lower bounds
CP   S_U       I    DP     slack variables for upper bounds
CP   CSCALE    I    DP     scaling factors for constraints
CP   M         I    INT    number of constraints
CP   LAM       I    DP     values of Lagrangian multipliers
CP   VIN       I    DP     vector to be multiplied with Hessian
CP                            (ordered like X)
CP   VOUT      I    DP     resulting product
CP                            (ordered like X)
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
CS    EVAL_HESSLAG_V
CS    EVAL_HESSOBJ_V
CS    CONSTR
CS    C_OUT
CS    IDAMAX
CS    DCOPY
CS    DAXPY
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
      integer ITER
      integer N
      integer NIND
      integer NFIX
      integer IFIX(NFIX)
      double precision X(N)
      integer IVAR(N)
      integer NORIG
      double precision XORIG(NORIG)
      integer NLB
      integer ILB(NLB)
      integer NUB
      integer IUB(NUB)
      double precision S_L(NLB)
      double precision S_U(NUB)
      double precision CSCALE(*)
      integer M
      double precision LAM(M)
      double precision VIN(N)
      double precision VOUT(N)
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
      integer IDAMAX
      double precision DNRM2

      integer i, k, p_rwend, p_iwend, p_vino, p_vouto, p_tmp, task
      integer p_xtmp, idummy
      double precision h, vnrm, v, times, timef
      character*80 line

      integer LAST_ITER
      save    LAST_ITER

      logical HESS_TOGETHER     ! if .true. Hessian is evaluated in one batch
                                ! otherwise first Hessian for constraints
                                ! (using CONSTR) and then for objective function
      logical QHESSFINDIFF
C
C*******************************************************************************
C
C                           Executable Statements
C
C*******************************************************************************
C
      call TIMER(times)
CTODO Could clean this up a little...
      QHESSFINDIFF = (QHESS.eq.2)
      HESS_TOGETHER = (QHESS.eq.0)
C
C     For Iter = -1 initialize LAST_ITER
C
      if( ITER.eq.-1 ) then

         LAST_ITER = -1
C
C     Otherwise, obtain Hessian and reorder accoring to IVAR (at the same
C     time, entries for fixed variables are eliminated)
C
      else
         COUNT_HV = COUNT_HV + 1

         IERR = 0
         p_iwend = 0
         p_rwend = 0
C
         if( ITER.ne.LAST_ITER ) then
            do i = 1, N
               XORIG(IVAR(i)) = X(i)
            enddo
            task = 0
         else
            task = 1
         endif
C
C     Get work space
C
         p_vino  = p_rwend
         p_vouto = p_vino  + NORIG
         p_rwend = p_vouto + NORIG
         if( p_rwend.gt.LRW ) then
            IERR = 98
            goto 9999
         endif
C
C     Reorder VIN according to IVAR
C
         if( NFIX.gt.0 ) then
            call DCOPY(NORIG, 0d0, 0, RW(p_vino+1), 1)
         endif
         do i = 1, N
            RW(p_vino+IVAR(i)) = VIN(i)
         enddo
C
C     Compute the product
C
         if( HESS_TOGETHER ) then
C
C     Take care of scaling
C
            if( QFSCALE.ne.1d0 ) then
               if( QFSCALE.eq.0d0 ) then
                  call C_OUT(2,0,1,'get_hv: QFSCALE is zero. Abort.')
                  IERR = 4
                  goto 9999
               endif
               call DSCAL(M, 1d0/QFSCALE, LAM, 1)
            endif
            if( QSCALE.eq.2 ) then
               do i = 1, M
                  LAM(i) = LAM(i)*CSCALE(i)
               enddo
            elseif( M.gt.0 .and. CSCALE(1).ne.1d0 ) then
               call DSCAL(M, CSCALE(1), LAM, 1)
            endif
C
C     Form the product
C
            call EVAL_HESSLAG_V(TASK, NORIG, XORIG, M, LAM,
     1           RW(p_vino+1), RW(p_vouto+1))
C
C     Unscale multipliers and scale result
C
            if( QFSCALE.ne.1d0 ) then
               call DSCAL(M, QFSCALE, LAM , 1)
               call DSCAL(NORIG, QFSCALE, RW(p_vouto+1), 1)
            endif
            if( QSCALE.eq.2 ) then
               do i = 1, M
                  LAM(i) = LAM(i)/CSCALE(i)
               enddo
            elseif( M.gt.0 .and. CSCALE(1).ne.1d0 ) then
               call DSCAL(M, 1d0/CSCALE(1), LAM, 1)
            endif

         else
C
C     Hessian of objective function
C
            call EVAL_HESSOBJ_V(TASK, NORIG, XORIG, M,
     1           RW(p_vino+1), RW(p_vouto+1))
            if( QFSCALE.ne.1d0 ) then
               call DSCAL(NORIG, QFSCALE, RW(p_vouto+1), 1)
            endif
C
C     Scale multipliers if necessary
C
            if( QSCALE.eq.2 ) then
               do i = 1, M
                  LAM(i) = LAM(i)*CSCALE(i)
               enddo
            elseif( M.gt.0 .and. CSCALE(1).ne.1d0 ) then
               call DSCAL(M, CSCALE(1), LAM, 1)
            endif
C
C     Hessian of the constraints
C
            p_tmp   = p_rwend
            p_rwend = p_tmp + NORIG
            if( p_rwend.gt.LRW ) then
               IERR = 98
               goto 9999
            endif
            if( QHESSFINDIFF ) then
C
C     Estimate Hessian vector product by finite difference
C     NOTE:  We assume that this will be premultiplied by Z, so that we don't
C     comute A * VIN  here!!!!!
C
C     Determine step size
C
c               i    = IDAMAX(NORIG, RW(p_vino+1), 1)
c               vnrm = dabs(RW(p_vino+i))
               vnrm = DNRM2(NORIG, RW(p_vino+1), 1)
               if( vnrm.eq.0.d0 ) then
                  call DCOPY(N, 0.d0, 0, VOUT, 1)
                  goto 9999
               else
                  h = 1d-6/vnrm
c                  h = 1d-6/dmin1(vnrm, 1d0)
c                  h = 1d-6
C
C     Make sure that no bounds are violated
C
c                  goto 1444
                  do i = 1, NLB
                     k = ILB(i)
                     v = RW(p_vino+IVAR(k))
                     if( v.lt.0.d0 ) then
                        h = dmin1(h, -S_L(i)/v)
                     endif
                  enddo
                  do i = 1, NUB
                     k = IUB(i)
                     v = RW(p_vino+IVAR(k))
                     if( v.gt.0.d0 ) then
                        h = dmin1(h, S_U(i)/v)
                     endif
                  enddo
 1444             continue
C
C     Compute tmp = A(x+h VIN) * LAM
C     (NOTE: We assume that 
C
                  p_xtmp  = p_rwend
                  p_rwend = p_xtmp + NORIG
                  if( p_rwend.gt.LRW ) then
                     IERR  = 98
                     goto 9999
                  endif
                  call DCOPY(NORIG, XORIG, 1, RW(p_xtmp+1), 1)
                  call DAXPY(NORIG, h, RW(p_vino+1), 1, RW(p_xtmp+1), 1)
C
                  call CONSTR(13, ITER, N, NIND, M, IVAR, NFIX, IFIX,
     1                 NORIG, RW(p_xtmp+1), CSCALE, LAM, RW(p_tmp+1),
     3                 idummy, idummy, KCONSTR(1), RS(KCONSTR(2)+1),
     4                 KCONSTR(3), IS(KCONSTR(4)+1), LRW-p_rwend,
     5                 RW(p_rwend+1), LIW-p_iwend, IW(p_iwend+1), IERR)
                  if( IERR.lt.0 ) then
                     write(line,*)
     1                    'get_hv: Warning in CONSTR-13, IERR = ',IERR
                     call C_OUT(2,0,1,line)
                  elseif( IERR.ne.0 ) then
                     write(line,*)
     1                    'get_hv: Error in CONSTR-13, IERR = ',IERR
                     call C_OUT(2,0,1,line)
                     goto 9999
                  endif
                  p_rwend = p_xtmp
                  call DSCAL(NORIG, 1.d0/h, RW(p_tmp+1), 1)
               endif

            else
               call CONSTR(20, ITER, N, NIND, M, IVAR, NFIX, IFIX,
     1              NORIG, XORIG, LAM, RW(p_vino+1), RW(p_tmp+1),
     3              idummy, idummy, KCONSTR(1), RS(KCONSTR(2)+1),
     4              KCONSTR(3), IS(KCONSTR(4)+1), LRW-p_rwend,
     5              RW(p_rwend+1), LIW-p_iwend, IW(p_iwend+1), IERR)
               if( IERR.lt.0 ) then
                  write(line,*)
     1                 'get_hv: Warning in CONSTR-20, IERR = ',IERR
                  call C_OUT(2,0,1,line)
               elseif( IERR.ne.0 ) then
                  write(line,*)
     1                 'get_hv: Error in CONSTR-20, IERR = ',IERR
                  call C_OUT(2,0,1,line)
                  goto 9999
               endif
            endif
C
C     Unscale multipliers
C
            if( QSCALE.eq.2 ) then
               do i = 1, M
                  LAM(i) = LAM(i)/CSCALE(i)
               enddo
            elseif( M.gt.0 .and. CSCALE(1).ne.1d0 ) then
               call DSCAL(M, 1d0/CSCALE(1), LAM, 1)
            endif
C
C     Add the two products
C
            call DAXPY(NORIG, 1d0, RW(p_tmp+1), 1, RW(p_vouto+1), 1)
            p_rwend = p_tmp
         endif
C
C     Reorder VOUT according to IVAR
C
         do i = 1, N
            VOUT(i) = RW(p_vouto+IVAR(i))
         enddo
         p_rwend = p_vino
C
         LAST_ITER = ITER
C
      endif
C
C     I think that's it...
C
 9999 continue
      call TIMER(timef)
      TIME_HV  = TIME_HV + timef - times
      return
      end
      
