/*
 * Tcl interface to the profiles, algebras and enumeration stuff
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

#ifndef TPROFILE_DEF
#define TPROFILE_DEF

#include <tcl.h>
#include "profile.h"

/* ids for our TPtr types */
#define TP_PROCORE 29
#define TP_PROFILE 30
#define TP_EXMON   31
#define TP_ENENV   32
#define TP_SQINF   33

int Tprofile_Init(Tcl_Interp *ip);

#endif
