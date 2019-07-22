C Copyright (C) 2000, International Business Machines
C Corporation and others.  All Rights Reserved.
       SUBROUTINE MTERMS(KAPPA , B   , H, LAFLA , POLY ,
     *                   PNTINT, NIND, N, NEQCON, LPOLY, LPTINT)


*    
*  ***********************************************************************
*  THIS SUBROUTINE COMPUTES THE TERMS OF THE QUADRATIC
*  INTERPOLATING POLYNOMIAL 
*                     T         T
*      M(X)= KAPPA + G X + 0.5*X H X
*
*  USING FINITE DIFFERENCES, FOUND IN 'FG'
*
*  PARAMETERS:
*  
*   N      (INPUT)  DIMENTION OF THE PROBLEM
*
*   NIND   (INPUT)  NUMBER OF INTERPOLATION POINTS
* 
*   PNTINT (INPUT)  LIST OF  'NIND' DATA POINTS. THE I-TH POINT OCCUPIES
*                   POSITIONS ( I - 1 ) * N + 1 TO I * N.
*   POLY   (INPUT)  THE ARRAY CONTAINING COEFFICIENTS OF NEWTON FUNDAMENTAL
*                   POLYNOMIALS (AS COMPUTED BY 'NBUILD')
*   LAFLA  (INPUT)  THE ARRAY OF FINITE DIFFERENCES
*
*   NEQCON (INPUT)  NUMBER OF LINEARLY INDEP. EQUALITY CONSTRAINTS
* 
*   KAPPA  (OUTPUT) THE CONSTANT TERM OF THE INTERPOLATION MODEL.
*
*   G      (OUTPUT) VECTOR OF THE LINEAR TERMS OF THE  INTERPOLATION MODEL.
*
*   H      (OUTPUT) MATRIX OF QUADRATIC TERMS OF THE  INTERPOLATION MODEL.
*
*  **************************************************************************
*



      DOUBLE PRECISION KAPPA, B(N), H(N,N), LAFLA(NIND),
     +                 POLY(LPOLY), PNTINT(LPTINT)

      INTEGER          NIND,N, LPOLY, LPTINT, NEQCON

C 
C  LOCAL VARIABLES
C
  
      INTEGER          NP1, DD, NDD, NINDM1, NDMNP1,
     +                 I, J, K, KK, KM, JJ

             


       NP1    = N + 1 
       DD     = (NP1)*(N+2)/2
       NDD    = DD-NP1
       NINDM1 = NIND  - 1 
       NDMNP1 = NINDM1 - N 


C
C  SET THE COEFFICIENT OF THE INTERPOLATION TO ZERO
C


       CALL RZRMAT(H, N, N)
       CALL RZRVEC(B, N)
C
C  COMPUTE THE CONSTANT TERM
C

C
C  INITIALIZE USING CONSTANT BLOCK
C
       KAPPA=LAFLA(1)*POLY(1)

C
C  UPDATE USING THE FINITE DIFFERENCES CORRESPONDING TO LINEAR BLOCK
C

       K=MIN(N-NEQCON,NINDM1)
       DO 10 J=1,K
         KAPPA = KAPPA + LAFLA(J+1)*POLY(2 + NP1*(J-1))
 10    CONTINUE

       IF (NIND.GT.NP1) THEN
C
C  UPDATE USING THE FINITE DIFF. CORRESPONDING TO QUADRATIC BLOCK
C
         K=MIN(NDD,NDMNP1)
         DO 20 J=1,K
           KAPPA = KAPPA + LAFLA(J+NP1)*POLY(2 + NP1*N + DD*(J-1))
 20      CONTINUE
       ENDIF


       
       DO 40 I = 1,N
C
C  COMPUTE DEGREE ONE TERMS
C
         IF (NIND.GT.1) THEN 

C
C  UPDATE USING LINEAR BLOCK
C     
           K=MIN(N-NEQCON,NINDM1)
           DO 50 J = 1,K
             B(I) = B(I) + LAFLA(J+1)*POLY(I + 2 + NP1*(J-1))
 50        CONTINUE
         ENDIF

         IF (NIND .GT.NP1) THEN
C
C  UPDATE USING QUADRATIC BLOCK
C
           K=MIN(NDD,NDMNP1)
           DO 60 J = 1,K
             B(I) = B(I) + LAFLA(J+NP1)*POLY(I + NP1*N + 2 + DD*(J-1))
 60        CONTINUE
         ENDIF
    
    
 40    CONTINUE



C
C  COMPUTE QUADRATIC TERMS USING QUADRATIC BLOCK
C

       IF (NIND.GT.NP1) THEN

C
C  POINTER TO FIRST QUADRATIC  BLOCK IN  'POLY'
C
   
         JJ = NP1*NP1 + 2

C
C  LOOP OVER THE NUMBER OF SECOND DEGREE POLYNOMIALS
C
         
         KM=MIN(NDD,NDMNP1)
         DO 80 J = 1,KM
           K=1
           DO 90 I = 1,N
C
C  UPDATE DIAGONAL ELEMENT
C
             H(I,I) = H(I,I) + 2.0D0*LAFLA(J+NP1)*
     +                         POLY(JJ+K-1+DD*(J-1))
             K=K+1
C
C  UPDATE OFF-DIAGONAL ELEMENTS
C
             DO 91 KK = I+1,N
               H(I,KK) = H(I,KK) + LAFLA(J+NP1)*POLY(JJ+K-1+DD*(J-1))
               H(KK,I) = H(I,KK)
               K=K+1
 91          CONTINUE
 90        CONTINUE
 80      CONTINUE
       ENDIF
  


       RETURN
       END









