/*
 * Generic pointers for the Tcl interface 
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

#ifndef TCL_TPDEF
#define TCL_TPDEF

#include <tcl.h>
#include <stdarg.h>

/* 
 * A TPtr holds two values: 
 *   type:      numerical type id  
 *   pointer:   an arbitrary (void *) 
 */
Tcl_ObjType TPtr;
 
/* Initialize this library */
int Tptr_Init(Tcl_Interp *ip);

/* Retrieve values from a TPtr object */
void *TPtr_GetPtr(Tcl_Obj *obj);
int   TPtr_GetType(Tcl_Obj *obj);

/* Create new TPtr object */
Tcl_Obj *Tcl_NewTPtr(int type, void *ptr);

/* Register the name of a type. This only improves the error messages 
 * generated by TPtr_CheckArgs. It also helps to ensure that type ids
 * are unique. */
void TPtr_RegType(int type, const char *name);

/* Predefined types; these use negative values */
#define TP_INT       -1    /* Tcl_Int */
#define TP_STRING    -2    /* Create up to date string rep. */
#define TP_PTR       -3    /* any TPtr is allowed */

/* More predefined types, which are actually instructions for TPtr_CheckArgs */
#define TP_END       -1111 /* end of argument list */
#define TP_ANY       -10   /* allow anything: argument isn't touched. */
#define TP_VARARGS   -11   /* allow arbitrary args from this point on */
#define TP_OPTIONAL  -100  /* following args are optional */
#define TP_MANDATORY -101  /* following args are mandatory (default) */

/* Check whether the provided array of args conforms to the 
 * types given as extra args. On failure it leaves an error message 
 * in ip's result variable. 
 */
int TPtr_CheckArgs(Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[], ...);

/* a few helper functions & macros */
Tcl_Obj *Tcl_ListFromArray(int *list, int len) ;

#define ENSUREARGS0 \
if (TCL_OK!=TPtr_CheckArgs(ip,objc,objv,TP_END)) return TCL_ERROR;
#define ENSUREARGS1(T1) \
if (TCL_OK!=TPtr_CheckArgs(ip,objc,objv,T1,TP_END)) return TCL_ERROR;
#define ENSUREARGS2(T1,T2) \
if (TCL_OK!=TPtr_CheckArgs(ip,objc,objv,T1,T2,TP_END)) return TCL_ERROR;
#define ENSUREARGS3(T1,T2,T3) \
if (TCL_OK!=TPtr_CheckArgs(ip,objc,objv,T1,T2,T3,TP_END)) return TCL_ERROR;
#define ENSUREARGS4(T1,T2,T3,T4) \
if (TCL_OK!=TPtr_CheckArgs(ip,objc,objv,T1,T2,T3,T4,TP_END)) return TCL_ERROR;

#endif

