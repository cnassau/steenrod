/*
 * Tcl interface to the enumerator structure
 *
 * Copyright (C) 2004-2007 Christian Nassau <nassau@nullhomotopie.de>
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

enumerator *Tcl_EnumFromObj(Tcl_Interp *ip, Tcl_Obj *obj); 

int Tenum_Init(Tcl_Interp *ip) ;

#endif

