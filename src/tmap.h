/*
 * Tcl interface to the implementation of maps
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

#ifndef TMAP_DEF
#define TMAP_DEF

#include <tcl.h>
#include "tptr.h"

/* our types */
#define TP_MAP    42
#define TP_MAPGEN 43
#define TP_MAPSUM 44

int TMap_Init(Tcl_Interp *ip) ;

#endif
