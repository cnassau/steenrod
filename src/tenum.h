/*
 * Tcl interface to the enumerator structure
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

#ifndef TENUM_DEF
#define TENUM_DEF

#include <tcl.h>
#include "enum.h"

#define TP_ENUM 27

int Tenum_Init(Tcl_Interp *ip) ;

#endif
