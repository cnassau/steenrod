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

/* our poly type */
#define TP_POLY 19

int Tpoly_Init(Tcl_Interp *ip) ;

#endif
