/*
 *  Generic pointers for the Tcl interface 
 *
 *  Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "tptr.h"
#include <string.h>

/* implementation (put this into tptr.c later) */

#define TCLRETERR( ip, msg ) \
do { if ( NULL != ip ) Tcl_SetResult( ip, msg, TCL_VOLATILE ); return TCL_ERROR; } while (0)

#define TP_FORMAT "<%p:%p>"

void make_TPtr( Tcl_Obj *objPtr, int type, void *ptr ) {
  objPtr->typePtr = &TPtr;
  objPtr->internalRep.twoPtrValue.ptr1 = (void *) type;
  objPtr->internalRep.twoPtrValue.ptr2 = ptr;
  Tcl_InvalidateStringRep( objPtr );
}

void *TPtr_GetPtr( Tcl_Obj *obj ) { return obj->internalRep.twoPtrValue.ptr2; } 
int   TPtr_GetType( Tcl_Obj *obj ) { return (int) obj->internalRep.twoPtrValue.ptr1; } 

/* try to turn objPtr into a TPtr */
int TPtr_SetFromAnyProc( Tcl_Interp *interp, Tcl_Obj *objPtr ) {
  char *aux = Tcl_GetStringFromObj( objPtr, NULL );
  void *d1, *d2;
  if ( 2 != sscanf( aux, TP_FORMAT, &d1, &d2 ) ) 
    TCLRETERR( interp, "not recognized as TPtr, proper format = \"" TP_FORMAT "\"" ); 
  if ( NULL != objPtr->typePtr->freeIntRepProc ) 
    objPtr->typePtr->freeIntRepProc( objPtr );
  make_TPtr( objPtr, (int) d1, d2 ); 
  return TCL_OK;
}

/* recreate string representation */
void TPtr_UpdateStringProc( Tcl_Obj *objPtr ) {
  objPtr->bytes = ckalloc( 50 );
  sprintf( objPtr->bytes, TP_FORMAT, objPtr->internalRep.twoPtrValue.ptr1, objPtr->internalRep.twoPtrValue.ptr2 );
  objPtr->length = strlen( objPtr->bytes );
}

/* create copy */
void TPtr_DupInternalRepProc( Tcl_Obj *srcPtr, Tcl_Obj *dupPtr ) {
  make_TPtr( dupPtr, (int) srcPtr->internalRep.twoPtrValue.ptr1, srcPtr->internalRep.twoPtrValue.ptr2 );
}

int TPtr_Init( Tcl_Interp *ip ) {

  /* set up type and register */
  TPtr.name = "typed pointer";
  TPtr.freeIntRepProc = NULL;
  TPtr.dupIntRepProc = TPtr_DupInternalRepProc;
  TPtr.updateStringProc =  TPtr_UpdateStringProc;
  TPtr.setFromAnyProc = TPtr_SetFromAnyProc;
  Tcl_RegisterObjType( &TPtr );

  return TCL_OK;
}

Tcl_Obj *Tcl_NewTPtr( int type, void *ptr ) {
  Tcl_Obj *res = Tcl_NewObj( ); 
  make_TPtr( res, type, ptr );
  return res;
}

void printTypename( char *buf, int type ) {
    switch (type) {
	case TP_ANY    : sprintf( buf, "<anything>" ); break; 
	case TP_INT    : sprintf( buf, "<integer>" ); break; 
	case TP_STRING : sprintf( buf, "<string>" ); break; 
	case TP_PTR    : sprintf( buf, "<typed pointer>" ); break; 
	case TP_VARARGS : sprintf( buf, "<whatever...>"  ); break; 
	default : sprintf( buf, "<pointer of type %d>", type ); break;
    }
}

void ckArgsErr( Tcl_Interp *ip, char *name, va_list *ap, int pos, char *msg ) {
  char err[500], *wrk = err;
  char typename[100];
  int type; 
  int optional = 0;
  if ( NULL == ip )   return ;
  if ( NULL != msg )  wrk += sprintf( wrk, "%s", msg );
  else                wrk += sprintf( wrk, "problem with arg #%d", pos );
  if ( NULL != name ) wrk += sprintf( wrk, "\nusage: %s", name );
  while ( TP_END != (type = va_arg( *ap, int )) ) {
      if ( TP_OPTIONAL == type ) { 
	  optional = 1; 
	  wrk += sprintf( wrk, " [ " );
      }
      printTypename( typename, type );
      wrk += sprintf( wrk, " %s", typename );
  }
  if (optional) wrk += sprintf( wrk, " ]" );
  Tcl_SetResult( ip, err, TCL_VOLATILE );
}

#define CHCKARGSERR( msg ) \
do { va_end( ap ); va_start( ap, objv ); ckArgsErr( ip, Tcl_GetString( *objvorig ), &ap, pos, msg ); va_end( ap ); return TCL_ERROR; } while (0)

int TPtr_CheckArgs( Tcl_Interp *ip, int objc, Tcl_Obj * CONST objv[], ... ) {
  va_list ap;
  int type; 
  int pos = 0;
  int optional = 0;
  int aux;
  
  Tcl_Obj * CONST *objvorig = objv; /* backup copy */
  
  /* skip program name */
  objc--; objv++;

  va_start( ap, objv );

  for ( pos=1; TP_END != (type = va_arg( ap, int )); objc--, objv++, pos++ ) {
    /* process control args */
    if ( TP_VARARGS   == type ) { va_end( ap ); return TCL_OK; }
    if ( TP_OPTIONAL  == type ) { optional = 1; continue; }
    if ( TP_MANDATORY == type ) { optional = 0; continue; }

    if ( !objc ) { /* no more args available */
      if ( optional ) { va_end( ap ); return TCL_OK; }
      CHCKARGSERR( "too few arguments" ); 
    }
  
    /* check for type mismatch */

    switch (type) {
    case TP_ANY: 
    case TP_STRING:
      continue;
    case TP_INT:
      if ( TCL_OK != Tcl_GetIntFromObj( ip, *objv, &aux ) )
	CHCKARGSERR( NULL ); 
      continue;
    case TP_PTR:
      if ( TCL_OK != Tcl_ConvertToType( ip, *objv, &TPtr ) ) 
	CHCKARGSERR( NULL ); 
      continue;
    }    
    
    /* default : expect TPtr of given type */
    
    if ( TCL_OK != Tcl_ConvertToType( ip, *objv, &TPtr ) ) 
      CHCKARGSERR( NULL ); 
    if ( TPtr_GetType( *objv ) != type ) 
      CHCKARGSERR( NULL ); 

    /* Ok. Goto next arg */
  }

  if ( objc ) /* too many args */
    CHCKARGSERR( "too many arguments" ); 

  va_end( ap );

  return TCL_OK;
}


