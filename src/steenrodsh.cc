/*
 * Custom Tcl shell with built-in Steenrod library
 *
 * Copyright (C) 2005-2018 Christian Nassau <nassau@nullhomotopie.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifdef USE_TCL_STUBS
#  undef USE_TCL_STUBS
#endif

#include <tcl.h>

extern int Steenrod_Init(Tcl_Interp *ip);

int Tcl_SteenAppInit(Tcl_Interp *interp)
{
    if (Tcl_Init(interp) == TCL_ERROR) {
    return TCL_ERROR;
    }

    if(TCL_OK != Tcl_Eval(interp, "package require Steenrod " PACKAGE_VERSION))
        return TCL_ERROR;

    Tcl_Eval(interp, "namespace import steenrod::*");

    Tcl_Eval(interp, "if $::tcl_interactive {"
             "puts [format {"
             "Tcl shell with Steenrod algebra support"
             " (version %s, "
             "built " __DATE__ " " __TIME__ ")"
             "} [::steenrod::Version]]}");

    return TCL_OK;
}

int main(int argc, char **argv) {
    Tcl_Main(argc, argv, Tcl_SteenAppInit);
    return 0;
}
