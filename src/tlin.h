/*
 * Tcl interface for the linear algebra routines
 *
 * Copyright (C) 2004-2009 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef TLINALG_DEF
#define TLINALG_DEF

#include <tcl.h>
#include "tptr.h"
#include "tprime.h"
#include "linwrp.h"

/* the namespace for our commands */
#define NSP "steenrod::"

int Tcl_ConvertToVector(Tcl_Interp *ip, Tcl_Obj *obj);
int Tcl_ObjIsVector(Tcl_Obj *obj);
vectorType *vectorTypeFromTclObj(Tcl_Obj *obj);
void       *vectorFromTclObj(Tcl_Obj *obj);

int Tcl_ConvertToMatrix(Tcl_Interp *ip, Tcl_Obj *obj);
int Tcl_ObjIsMatrix(Tcl_Obj *obj);
matrixType *matrixTypeFromTclObj(Tcl_Obj *obj);
void       *matrixFromTclObj(Tcl_Obj *obj);

Tcl_Obj *Tcl_NewMatrixObj(matrixType *tp, void *data);
Tcl_Obj *Tcl_NewVectorObj(vectorType *tp, void *data);

/* ids of the types that we register */
#define TP_VECTOR 13
#define TP_MATRIX 16

int Tlin_Init(Tcl_Interp *ip);

#endif
