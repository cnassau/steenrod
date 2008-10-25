/*
 * Lemon based general purpose parser
 *
 * Copyright (C) 2009 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef LEPAR_DEF
#define LEPAR_DEF

#include <tcl.h>

typedef struct {
   void *theparser;
   Tcl_Interp *ip;
   Tcl_Obj *objv[10];
   int busy;
   int deleted;
   int failed;
   int tracing;
} Parser;

const char **LeParTokens;
int LeParNumTokens;

int Lepar_Init(Tcl_Interp *ip) ;

#endif
