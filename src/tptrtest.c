/*
 * Test file for the Tcl TPtr interface
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

/* Together with tptr.o this file constitutes a library libtptr.so which
 * can be loaded from Tcl. The file tptrtest.tcl then loads this library 
 * and carries out some basic tests. */

#include "tptr.h"

#define RETINTERR( msg ) do { \
  if ( NULL != ip ) \
     Tcl_SetResult( ip, "Internal error (" msg ") in tptr::create", TCL_VOLATILE ); \
  return TCL_ERROR; \
} while (0) 

int create( ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[] ) {
    int a,b;

    if ( TCL_OK != TPtr_CheckArgs( ip, objc, objv, TP_INT, TP_INT, TP_END ) ) 
	return TCL_ERROR;
    if ( TCL_OK != Tcl_GetIntFromObj( ip, objv[1], &a ) ) 
	RETINTERR( "#1" );
    if ( TCL_OK != Tcl_GetIntFromObj( ip, objv[2], &b ) ) 
	RETINTERR( "#2" );
    
    Tcl_SetObjResult( ip, Tcl_NewTPtr( a, (void *) b ) );
    return TCL_OK;
}

int Tptrtest_Init( Tcl_Interp *ip ) {
    
    Tcl_InitStubs( ip, "8.0", 0 ) ;   

    TPtr_Init( ip );

    Tcl_CreateObjCommand( ip, "tptr::create", create, 0, NULL );




    return TCL_OK;
}
