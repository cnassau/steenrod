/*
 * Tcl interface to the basic prime stuff
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

#ifndef TPRIME_DEF
#define TPRIME_DEF

#include <tcl.h>

/* id of our primeInfo type */
#define TP_PRINFO 11

int Tprime_Init(Tcl_Interp *ip);

#endif
