/*
 * OpenCL support routines
 *
 * Copyright (C) 2019-2019 Christian Nassau <nassau@nullhomotopie.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef STEENROD_OPENCL
#define STEENROD_OPENCL

#define CL_TARGET_OPENCL_VERSION 120

#include <CL/cl.h>
#include <tcl.h>
#include "poly.h"
#include <stddef.h>

#define MAXQUEUE 5

typedef struct {
  cl_platform_id pid;
  cl_device_id   did;
  cl_context     ctx;
  int            create_queues_with_profiling;
  cl_command_queue queue[MAXQUEUE];
} stcl_context;

typedef struct {
  cl_kernel ker;
  cl_device_id dev;
} stcl_kernel;

# define CLENUMDEF                               \
typedef struct {                                 \
    hostint  maxdeg[NALG];                           \
    hostint  profile[NALG];                          \
    hostint  tablen;                                 \
    hostint  gencnt;                                 \
    hostint *seqtab;   /* seqtab[NALG*tablen] */     \
    hostint *offsets;  /* offsets[gencnt] */         \
} __attribute__ ((packed)) clenum

#define hostint int

CLENUMDEF;

void SetCLErrorCode(Tcl_Interp *ip, cl_int errcode);

int makeWaitList(Tcl_Interp *ip, Tcl_Obj *o, int *numwait, cl_event **evtlist);

cl_event STcl_GetEvent(Tcl_Interp *ip, char *varname);
int STcl_SetEventTrace(Tcl_Interp *ip, char *varname, cl_event evt);
int STcl_SetEventCB(cl_event evt, Tcl_Obj *evtdesc);

Tcl_Obj *STcl_GetEventInfo(Tcl_Interp *ip, cl_event e);

int STcl_GetContext(Tcl_Interp *ip, stcl_context **ctx);

int STcl_GetKernelFromObj(Tcl_Interp *ip, Tcl_Obj *obj, stcl_kernel **kernel);
 
int STcl_GetContextFromObj(Tcl_Interp *ip, Tcl_Obj *obj, stcl_context **context);

int STcl_GetMemObjFromObj(Tcl_Interp *ip, Tcl_Obj *obj, cl_mem *memptr);

cl_command_queue GetOrCreateCommandQueue(Tcl_Interp *ip, stcl_context *ctx, int queue);

int GetMemFlagFromTclObj(Tcl_Interp *ip, Tcl_Obj *obj, int *ans);

int STcl_CreateMemObj(Tcl_Interp *ip, Tcl_Obj *obj, cl_mem mem);

int CL_Init(Tcl_Interp *ip);

#endif
