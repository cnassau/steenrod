/*
 * Tcl interface to the polynomial routines
 *
 * Copyright (C) 2004-2018 Christian Nassau <nassau@nullhomotopie.de>
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

Tcl_Obj *Tcl_NewExmoObj(exmo *ex);       /* keeps reference to *ex */
Tcl_Obj *Tcl_NewExmoCopyObj(exmo *ex);   /* creates private copy of *ex */

int Tcl_ConvertToPoly(Tcl_Interp *ip, Tcl_Obj *obj);
int Tcl_ObjIsPoly(Tcl_Obj *obj);
polyType *polyTypeFromTclObj(Tcl_Obj *obj);
void     *polyFromTclObj(Tcl_Obj *obj);

Tcl_Obj *Tcl_NewPolyObj(polyType *tp, void *data);

#if USEOPENCL
int Tcl_CLMapPoly(Tcl_Interp *ip, Tcl_Obj *polyobj, Tcl_Obj *lengthvar, Tcl_Obj *redbuf, Tcl_Obj *extbuf, Tcl_Obj *genbuf, Tcl_Obj *coeffbuf, int readonly);
#endif

/* our poly type */
#define TP_POLY  19
#define TP_EXMO  18

void exmoLog(const char *message,const exmo *exm);

int Tpoly_Init(Tcl_Interp *ip) ;

#endif
