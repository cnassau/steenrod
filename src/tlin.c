/*
 * Tcl interface for the linear algebra routines
 *
 * Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "tlin.h"

#define NSP "linalg::"

typedef enum {
    LIN_CREATE_VECTOR, LIN_DISPOSE_VECTOR, LIN_RANDOMIZE_VECTOR, 
    LIN_CREATE_MATRIX, LIN_DISPOSE_MATRIX, LIN_RANDOMIZE_MATRIX, 
    LIN_VECDIM, LIN_MATDIM, LIN_MATVECT, LIN_GETVECENT, LIN_GETMATENT, 
    LIN_SETVECENT, LIN_SETMATENT, LIN_MATORTH, LIN_MATLIFT, LIN_MATQUOT
} LinalgCmdCode;

#define RETERR( msg ) \
{ if (NULL!=ip) Tcl_SetResult(ip,msg,TCL_VOLATILE); return TCL_ERROR; }  

#define ENSURERANGE(bot,a,top) \
if (((a)<(bot))||((a)>=(top))) RETERR("index out of range"); 

#define ENSUREARGS1(T1) \
if (TCL_OK!=TPtr_CheckArgs(ip,objc,objv,T1,TP_END)) return TCL_ERROR;
#define ENSUREARGS2(T1,T2) \
if (TCL_OK!=TPtr_CheckArgs(ip,objc,objv,T1,T2,TP_END)) return TCL_ERROR;
#define ENSUREARGS3(T1,T2,T3) \
if (TCL_OK!=TPtr_CheckArgs(ip,objc,objv,T1,T2,T3,TP_END)) return TCL_ERROR;
#define ENSUREARGS4(T1,T2,T3,T4) \
if (TCL_OK!=TPtr_CheckArgs(ip,objc,objv,T1,T2,T3,T4,TP_END)) return TCL_ERROR;

int tLinComboCmd( ClientData cd, Tcl_Interp *ip, 
		  int objc, Tcl_Obj *CONST objv[] ) {
    LinalgCmdCode cdi = (LinalgCmdCode) cd;
    int a, b, c;
    primeInfo *pi;
    char *err;
    vector *vec;
    matrix *mat;
    Tcl_Obj *obp[2];

    if (NULL==ip) return TCL_ERROR; 

    switch (cdi) {
	case LIN_CREATE_VECTOR: 
	    ENSUREARGS1(TP_INT);
	    Tcl_GetIntFromObj( ip, objv[1], &a );
	    if (NULL== (vec = vector_create(a))) 
		RETERR( "Out of memory" );
	    Tcl_SetObjResult( ip, Tcl_NewTPtr( TP_VECTOR, vec ) );
	    return TCL_OK;
 	case LIN_CREATE_MATRIX: 
	    ENSUREARGS2(TP_INT,TP_INT);
	    Tcl_GetIntFromObj( ip, objv[1], &a );
	    Tcl_GetIntFromObj( ip, objv[2], &b );
	    if (NULL== (mat = matrix_create(a,b))) 
		RETERR( "Out of memory" );
	    Tcl_SetObjResult( ip, Tcl_NewTPtr( TP_MATRIX, mat ) );
	    return TCL_OK;
	case LIN_DISPOSE_VECTOR:
	    ENSUREARGS1(TP_VECTOR);
	    vec = (vector *) TPtr_GetPtr( objv[1] );
	    vector_dispose( vec );
	    return TCL_OK;
	case LIN_RANDOMIZE_VECTOR:
	    ENSUREARGS2(TP_VECTOR,TP_INT);
	    vec = (vector *) TPtr_GetPtr( objv[1] );
	    Tcl_GetIntFromObj( ip, objv[2], &a );
	    vector_randomize( vec, a );
	    return TCL_OK;
	case LIN_DISPOSE_MATRIX:
	    ENSUREARGS1(TP_MATRIX);
	    mat = (matrix *) TPtr_GetPtr( objv[1] );
	    matrix_destroy( mat );
	    return TCL_OK;
	case LIN_RANDOMIZE_MATRIX:
	    ENSUREARGS2(TP_MATRIX,TP_INT);
	    mat = (matrix *) TPtr_GetPtr( objv[1] );
	    Tcl_GetIntFromObj( ip, objv[2], &a );
	    matrix_randomize( mat, a );
	    return TCL_OK;
	case LIN_VECDIM:
	    ENSUREARGS1(TP_VECTOR);  
	    vec = (vector *) TPtr_GetPtr( objv[1] );
	    Tcl_SetObjResult( ip, Tcl_NewIntObj( vec->num ) );
	    return TCL_OK;
	case LIN_MATDIM: 
	    ENSUREARGS1(TP_MATRIX);  
	    mat = (matrix *) TPtr_GetPtr( objv[1] );
	    obp[0] = Tcl_NewIntObj( mat->rows );
	    obp[1] = Tcl_NewIntObj( mat->cols );
	    Tcl_SetObjResult( ip, Tcl_NewListObj( 2, obp ) );
	    return TCL_OK;
	case LIN_MATVECT: 
	    ENSUREARGS2(TP_MATRIX,TP_INT);
	    mat = (matrix *) TPtr_GetPtr( objv[1] );
	    Tcl_GetIntFromObj( ip, objv[2], &a );
	    ENSURERANGE(0,a,mat->rows);
	    RETERR("this command is not implemented yet");
	case LIN_GETVECENT:
	    ENSUREARGS2(TP_VECTOR,TP_INT);
	    vec = (vector *) TPtr_GetPtr( objv[1] );
	    Tcl_GetIntFromObj( ip, objv[2], &a );
	    ENSURERANGE(0,a,vec->num);
	    Tcl_SetObjResult(ip,Tcl_NewIntObj(vec->data[a]));
	    return TCL_OK;
	case LIN_GETMATENT:
	    ENSUREARGS3(TP_MATRIX,TP_INT,TP_INT);
	    mat = (matrix *) TPtr_GetPtr( objv[1] );
	    Tcl_GetIntFromObj( ip, objv[2], &a );
	    Tcl_GetIntFromObj( ip, objv[3], &b );
	    ENSURERANGE(0,a,mat->rows);
	    ENSURERANGE(0,b,mat->cols);
	    c = *(mat->data + a*(mat->nomcols)*sizeof(cint) + b);
	    Tcl_SetObjResult(ip,Tcl_NewIntObj( c ));
	    return TCL_OK;
	case LIN_SETVECENT:
	    ENSUREARGS3(TP_VECTOR,TP_INT,TP_INT);
	    vec = (vector *) TPtr_GetPtr( objv[1] );
	    Tcl_GetIntFromObj( ip, objv[2], &a );
	    ENSURERANGE(0,a,vec->num);
	    vec->data[a] = b;
	    return TCL_OK;
	case LIN_SETMATENT: 
	    ENSUREARGS4(TP_MATRIX,TP_INT,TP_INT,TP_INT);
	    mat = (matrix *) TPtr_GetPtr( objv[1] );
	    Tcl_GetIntFromObj( ip, objv[2], &a );
	    Tcl_GetIntFromObj( ip, objv[3], &b );
	    Tcl_GetIntFromObj( ip, objv[4], &c );
	    ENSURERANGE(0,a,mat->rows);
	    ENSURERANGE(0,b,mat->cols);
	    *(mat->data + a*(mat->nomcols)*sizeof(cint) + b) = c;
	    return TCL_OK;
	case LIN_MATORTH: 
	case LIN_MATLIFT: 
	case LIN_MATQUOT:   
    }

    return TCL_OK;
}

int Tlin_Init( Tcl_Interp *ip ) {

    if ( NULL == Tcl_InitStubs( ip, "8.0", 0 ) ) return TCL_ERROR;

    Tprime_Init( ip );
    

#define CREATECOMMAND( name, code) \
Tcl_CreateObjCommand(ip,NSP name,tLinComboCmd,(ClientData) code, NULL);

    TPtr_RegType( TP_VECTOR, "vector" );
    TPtr_RegType( TP_MATRIX, "matrix" );

    /* basic commands */
    CREATECOMMAND( "createVector", LIN_CREATE_VECTOR);
    CREATECOMMAND( "disposeVector", LIN_DISPOSE_VECTOR);
    CREATECOMMAND( "randomizeVector", LIN_RANDOMIZE_VECTOR);
    CREATECOMMAND( "createMatrix", LIN_CREATE_MATRIX);
    CREATECOMMAND( "disposeMatrix", LIN_DISPOSE_MATRIX);
    CREATECOMMAND( "randomizeMatrix", LIN_RANDOMIZE_MATRIX);
    CREATECOMMAND( "getVectorDim", LIN_VECDIM);
    CREATECOMMAND( "getMatrixDim", LIN_MATDIM);
    CREATECOMMAND( "createMatVect", LIN_MATVECT);
    CREATECOMMAND( "getVectEntry", LIN_GETVECENT);
    CREATECOMMAND( "getMatEntry", LIN_GETMATENT);
    CREATECOMMAND( "setVectEntry", LIN_SETVECENT);
    CREATECOMMAND( "setMatEntry", LIN_SETMATENT);

    /* advanced stuff*/
    CREATECOMMAND( "matrixOrtho", LIN_MATORTH);
    CREATECOMMAND( "matrixLift", LIN_MATLIFT);
    CREATECOMMAND( "matrixQuotient", LIN_MATQUOT);

    return TCL_OK;
}

