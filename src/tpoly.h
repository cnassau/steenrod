/*
 * Tcl interface to the polynomial routines
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

#ifndef TPOLY_DEF
#define TPOLY_DEF

#include <tcl.h>
#include "tptr.h"
#include "poly.h"

int Tcl_ConvertToExmo(Tcl_Interp *ip, Tcl_Obj *obj);
int Tcl_ObjIsExmo(Tcl_Obj *obj);
exmo *exmoFromTclObj(Tcl_Obj *obj); 

int Tcl_ConvertToPoly(Tcl_Interp *ip, Tcl_Obj *obj);
int Tcl_ObjIsPoly(Tcl_Obj *obj);
polyType *polyTypeFromTclObj(Tcl_Obj *obj); 
void     *polyFromTclObj(Tcl_Obj *obj); 

Tcl_Obj *Tcl_NewPolyObj(polyType *tp, void *data);

/* our poly type */  
#define TP_POLY  19  
#define TP_EXMO  18 

int Tpoly_Init(Tcl_Interp *ip) ;

#endif
