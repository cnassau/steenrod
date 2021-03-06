/*
 * Monomaps - implements a map from monomials to Tcl_Objects
 *
 * Copyright (C) 2004-2018 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef MOMA_DEF
#define MOMA_DEF

#include <tcl.h>
#include "poly.h"

typedef struct {
    void     *keys;       /* a stdpoly polynomial */
    Tcl_Obj **values;     /* array of Tcl_Obj */
    int       valloc;     /* space allocated for values */
    Tcl_HashTable *tab;
} momap;

momap *momapCreate(void); 
void   momapClear(momap *mo);
void   momapDestroy(momap *mo);

Tcl_Obj *momapGetValPtr(momap *mo, Tcl_Obj *key);
int momapSetValPtr(momap *mo, Tcl_Obj *key, Tcl_Obj *val);

int Momap_Init(Tcl_Interp *ip) ;

momap *Tcl_MomapFromObj(Tcl_Interp *ip, Tcl_Obj *obj); 

#endif

