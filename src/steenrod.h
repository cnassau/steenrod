/*
 * Main entry point to the Steenrod library
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

#ifndef STEENROD_DEF
#define STEENROD_DEF

#include <tcl.h>
#include "momap.h"
#include "tenum.h"
#include "linwrp.h"

#ifndef STEENROD_C
extern char *theprogvar; /* ckalloc'ed name of the progress variable */
extern int   theprogmsk; /* progress reporting granularity */
#endif

#define THEPROGVAR ((*theprogvar) ? theprogvar : NULL)

int Steenrod_Init(Tcl_Interp *ip) ;

#endif

