/*
 * Main entry point to the steenrod library
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

#include <tcl.h>
#include "tprime.h"
#include "tpoly.h"
#include "tlin.h"
#include "steenrod.h"

int Steenrod_Init(Tcl_Interp *ip) {

    Tcl_InitStubs(ip, "8.0", 0);

    Tptr_Init(ip);
    Tprime_Init(ip);
    Tpoly_Init(ip);
    Momap_Init(ip);
    Tenum_Init(ip);
    Tlin_Init(ip);

    Tcl_PkgProvide(ip, "Steenrod", "1.0");

    return TCL_OK;
}
