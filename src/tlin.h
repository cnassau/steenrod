/*
 * Tcl interface for the linear algebra routines
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

#ifndef TLINALG_DEF
#define TLINALG_DEF

#include <tcl.h>
#include "tptr.h"
#include "tprime.h"
#include "adlin.h"

/* ids of the types that we register */
#define TP_VECTOR 13
#define TP_MATRIX 16

int Tlin_Init(Tcl_Interp *ip);

#endif
