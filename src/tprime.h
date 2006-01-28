/*
 * Tcl interface to the basic prime stuff
 *
 * Copyright (C) 2004-2006 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef TPRIME_DEF
#define TPRIME_DEF

#include <tcl.h>
#include "prime.h"

/* TPtr id of our "prime" type */
#define TP_PRIME 11

int Tprime_Init(Tcl_Interp *ip);

int Tcl_GetPrimeInfo(Tcl_Interp *ip, Tcl_Obj *obj, primeInfo **pi);

#endif
