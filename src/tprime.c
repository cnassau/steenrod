/*
 * Tcl interface to the basic prime stuff
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

#include "tptr.h"
#include "prime.h"

#define TP_PRINFO 11

/* client data to distinguish the subcommands */
#define CD_CREATE      1
#define CD_DISPOSE     2
#define CD_MAXDEG      3
#define CD_TPMO        4
#define CD_N           5
#define CD_PRIMPOWS    6
#define CD_REDDEGS     7
#define CD_EXTDEGS     8
#define CD_INVERSE     9
#define CD_BINOM       10
#define CD_PRIME       11

int tPrInfo( ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[] ) {
    int cdi = (int) cd;
    int a, b, c;
    primeInfo *pi;
    char *err;

    /* the initial switch just implements create & dispose;
     * for other commands we only check the arguments here */
    switch (cdi) {
	case CD_CREATE:
	    if ( TCL_OK != TPtr_CheckArgs( ip, objc, objv, TP_INT, TP_INT, TP_END ) )
		return TCL_ERROR;
	    Tcl_GetIntFromObj( ip, objv[1], &a );
	    Tcl_GetIntFromObj( ip, objv[2], &b );
	    pi = (primeInfo *) ckalloc( sizeof(primeInfo) ); 
	    if ( NULL == pi ) {
		Tcl_SetResult( ip, "out of memory", TCL_VOLATILE );
		return TCL_ERROR;
	    }
	    if ( PI_OK == (c = makePrimeInfo( pi, a, b )) ) {
		Tcl_SetObjResult( ip, Tcl_NewTPtr( TP_PRINFO, pi ) );
		return TCL_OK;
	    }
	    switch (c) {
		case PI_NOPRIME:  err = "Not a prime number"; break;
		case PI_TOOLARGE: err = "Prime too large"; break;
		case PI_NOMEM:    err = "Out of memory"; break;
		default: err = "<strange internal error>"; 
	    }
	    Tcl_SetResult( ip, err, TCL_VOLATILE );
	    return TCL_ERROR;
	case CD_DISPOSE:
	    if ( TCL_OK != TPtr_CheckArgs( ip, objc, objv, TP_PRINFO, TP_END ) )
		return TCL_ERROR;
	    disposePrimeInfo( pi = (primeInfo *) TPtr_GetPtr( objv[1] ) );
	    ckfree( (char *) pi );
	    return TCL_OK;
	case CD_INVERSE:
	    if ( TCL_OK != TPtr_CheckArgs( ip, objc, objv, TP_PRINFO, TP_INT, TP_END ) )
		return TCL_ERROR;
	    Tcl_GetIntFromObj( ip, objv[2], &a );
	    break;
	case CD_BINOM:
	    if ( TCL_OK != TPtr_CheckArgs( ip, objc, objv, TP_PRINFO, TP_INT, TP_INT, TP_END ) )
		return TCL_ERROR;
	    Tcl_GetIntFromObj( ip, objv[2], &a );
	    Tcl_GetIntFromObj( ip, objv[3], &b );
	    break;
	default:
	    if ( TCL_OK != TPtr_CheckArgs( ip, objc, objv, TP_PRINFO, TP_END ) )
		return TCL_ERROR;
    }
    
    pi = (primeInfo *) TPtr_GetPtr( objv[1] );
    
#define RETURNINT( rval ) Tcl_SetObjResult( ip, Tcl_NewIntObj( rval ) ); return TCL_OK
#define RETURNLIST( list, len ) Tcl_SetObjResult( ip, Tcl_ListFromArray( list, len ) ); return TCL_OK

    switch (cdi) {
	case CD_BINOM: RETURNINT( binomp( pi, a, b )) ;
	case CD_INVERSE:
	    a %= pi->prime;
	    if ( 0 == a) {
		Tcl_SetResult( ip, "division by zero", TCL_VOLATILE );
		return TCL_ERROR;
	    }
	    RETURNINT( pi->inverse[a] );
	case CD_PRIME: RETURNINT( pi->prime );
	case CD_MAXDEG: RETURNINT( pi->maxdeg );
	case CD_N: RETURNINT( pi->N );
	case CD_TPMO: RETURNINT( pi->tpmo );
	case CD_REDDEGS: RETURNLIST( pi->reddegs, pi->N );
	case CD_EXTDEGS: RETURNLIST( pi->extdegs, pi->N );
	case CD_PRIMPOWS: RETURNLIST( pi->primpows, pi->N );
    } 
    
    Tcl_SetResult( ip, "internal error in tPtrInfo", TCL_VOLATILE );
    return TCL_ERROR;
}

/* our namespace */
#define NSP "primeinfo::"

int Tprime_Init( Tcl_Interp *ip ) {

    Tcl_InitStubs( ip, "8.0", 0 );

    TPtr_Init( ip );

    TPtr_RegType( TP_PRINFO, "primeinfo" );

    Tcl_CreateObjCommand( ip, NSP "create",  tPrInfo, (ClientData) CD_CREATE, NULL );
    Tcl_CreateObjCommand( ip, NSP "dispose", tPrInfo, (ClientData) CD_DISPOSE, NULL );

    Tcl_CreateObjCommand( ip, NSP "prime", tPrInfo, (ClientData) CD_PRIME, NULL );
    Tcl_CreateObjCommand( ip, NSP "maxdeg", tPrInfo, (ClientData) CD_MAXDEG, NULL );
    Tcl_CreateObjCommand( ip, NSP "tpmo", tPrInfo, (ClientData) CD_TPMO, NULL );
    Tcl_CreateObjCommand( ip, NSP "N", tPrInfo, (ClientData) CD_N, NULL );
    Tcl_CreateObjCommand( ip, NSP "primpows", tPrInfo, (ClientData) CD_PRIMPOWS, NULL );
    Tcl_CreateObjCommand( ip, NSP "reddegs", tPrInfo, (ClientData) CD_REDDEGS, NULL );
    Tcl_CreateObjCommand( ip, NSP "extdegs", tPrInfo, (ClientData) CD_EXTDEGS, NULL );
    Tcl_CreateObjCommand( ip, NSP "inverse", tPrInfo, (ClientData) CD_INVERSE, NULL );
    Tcl_CreateObjCommand( ip, NSP "binom", tPrInfo, (ClientData) CD_BINOM, NULL );
    
    return TCL_OK;
}
