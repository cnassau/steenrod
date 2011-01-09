/*
 * Basic Open CL support
 *
 * Copyright (C) 2004-2009 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef OPCL_DEF
#define OPCL_DEF

#include <tcl.h>

#ifdef HAVE_CL_CL_H
#  include <CL/cl.h>
#else
#  include <OpenCL/cl.h>
#endif

const char *clerrorstring(cl_int errcode);

typedef struct {
   cl_platform_id pid;
   cl_device_id   did;
   cl_context     ctx;
   cl_program     prg;
} CLCTX;

CLCTX *GetCLCtx(Tcl_Interp *ip);

int OPCL_Init(Tcl_Interp *ip) ;

#endif

