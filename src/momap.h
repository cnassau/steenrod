/*
 * Monomaps - implements a map from monomials to Tcl_Objects
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

#ifndef MOMA_DEF
#define MOMA_DEF

#include <tcl.h>
#include "poly.h"

typedef struct {
    void     *keys;       /* a stdpoly polynomial */
    Tcl_Obj **values;     /* array of Tcl_Obj */
    int       valloc;     /* space allocated for values */
} momap;

momap *momapCreate(void); 
void   momapClear(momap *mo);
void   momapDestroy(momap *mo);

Tcl_Obj **momapGetValPtr(momap *mo, const exmo *key);
int momapSetValPtr(momap *mo, const exmo *key, Tcl_Obj *val);

int Momap_Init(Tcl_Interp *ip) ;

#endif

