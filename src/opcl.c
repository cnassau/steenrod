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

#define OPCL_INCLUDES 1

#include "opcl.h"
#include <stdio.h>

#include "tenum.h"


void DestroyCLMatrix(cl_matrix *res) {
    if(res->hostbuf) free(res->hostbuf);
    if(res->buffer) clReleaseMemObject(res->buffer);    
    free(res);
}

cl_matrix *CreateCLMatrix(Tcl_Interp *ip, int rows, int cols, int prime) {
    cl_int errc;
    CLCTX *ctx = GetCLCtx(ip);
    cl_matrix *res = (cl_matrix *) ckalloc(sizeof(cl_matrix));
    if(NULL == res) return NULL;
    res->rows = rows;
    res->cols = cols;
    res->prime = prime;
    res->bytesperrow = cols;
    res->size = res->bytesperrow * res->rows;
    if(0 == res->size) res->size=1;
    res->ctx = ctx;
    res->hostbuf = NULL;
    res->buffer = clCreateBuffer(ctx->ctx,
                                 CL_MEM_READ_WRITE,
                                 res->size,
                                 NULL,
                                 &errc);
    if(CL_SUCCESS != errc) {
         DestroyCLMatrix(res);
         return NULL;
    }        
    return res;
}





const char *clerrorstring(errorcode) {
#define errcode(code,txt) {if(code==errorcode){return txt;} }
#include "opclerrcodes.c"
   return "unknown cl error code";
}

void TclCLError(Tcl_Interp *ip, cl_uint errorcode) {
   char emsg[200];
   snprintf(emsg,200,"open cl error code %s", clerrorstring(errorcode));
   Tcl_SetResult(ip,emsg,TCL_VOLATILE);
}


static CLCTX *ctx_global;

CLCTX *GetCLCtx(Tcl_Interp *ip) {
    return ctx_global;
}


int AddPlatformInfo(Tcl_Obj *x, cl_platform_id pid) {
   Tcl_Obj *p = Tcl_NewObj();
   char pval[1000];
   
#if 0    
   Tcl_ListObjAppendElement(NULL,p,Tcl_NewStringObj("platform_id",-1));
   Tcl_ListObjAppendElement(NULL,p,Tcl_NewIntObj(pid));
#endif

#define REPPAR(par) \
{ if(CL_SUCCESS == clGetPlatformInfo(pid,par,1000,pval,NULL)) { \
   Tcl_ListObjAppendElement(NULL,p,Tcl_NewStringObj(#par,-1)); \
   Tcl_ListObjAppendElement(NULL,p,Tcl_NewStringObj(pval,-1)); }}
   

REPPAR(CL_PLATFORM_PROFILE);
REPPAR(CL_PLATFORM_VERSION);
REPPAR(CL_PLATFORM_NAME);
REPPAR(CL_PLATFORM_VENDOR);
REPPAR(CL_PLATFORM_EXTENSIONS);


   Tcl_ListObjAppendElement(NULL,x,p);
   return TCL_OK;
}

int CLEnmtestCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *const objv[]) {
   CLCTX *clc = (CLCTX *) cd;
   enumerator *enm;
   int obc, i;
   Tcl_Obj **obv;

   enm = Tcl_EnumFromObj(ip,objv[1]);
   if(NULL == enm) {
      return TCL_ERROR;
   }

   if( TCL_OK != Tcl_ListObjGetElements(ip,objv[2],&obc,&obv)) {
      return TCL_ERROR;
   }

    

   

   return TCL_OK;
}


int CLCombiCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *const objv[]) {
   CLCTX *clc = (CLCTX *) cd;

if(0==strcmp(Tcl_GetString(objv[1]),"program")) {
   const char *src = Tcl_GetString(objv[2]);
   cl_int err;
   clc->prg = clCreateProgramWithSource(clc->ctx,1,&src,NULL,&err);
   if(CL_SUCCESS != err) {
    TclCLError(ip,err);
return TCL_ERROR;
}

#define ckerr if(CL_SUCCESS != err) { TclCLError(ip,err);return TCL_ERROR;}
err = clBuildProgram(clc->prg,1,&(clc->did),NULL,NULL,NULL);
//ckerr;   

cl_build_status build_status;
 err = clGetProgramBuildInfo(clc->prg, clc->did,
   CL_PROGRAM_BUILD_STATUS, sizeof(cl_build_status), &build_status, NULL);


// if program built fails, print out error messages
 if (build_status != CL_SUCCESS) {
       char *build_log;
       size_t ret_val_size;
       err = clGetProgramBuildInfo(clc->prg, clc->did, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);
//       checkErr(err, "clGetProgramBuildInfo");
ckerr;
       build_log = malloc(ret_val_size+1);
       err = clGetProgramBuildInfo(clc->prg, clc->did, CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);
//       checkErr(err, "clGetProgramBuildInfo");
ckerr;
//fprintf(stderr,"rvs=%d\n",ret_val_size);
//fprintf(stderr,"blg=%s\n",build_log);

       // to be carefully, terminate with \0
       // there's no information in the reference whether the string is 0 terminated or not
       build_log[ret_val_size] = '\0';
//fprintf(stderr,build_log);
Tcl_SetResult(ip,"program compilation error:\n",TCL_STATIC);
Tcl_AppendResult(ip,build_log,NULL);
return TCL_ERROR;
 }



    return TCL_OK;
}


   return TCL_ERROR;
}

int CLInitCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *const objv[]) {

   CLCTX *clc = (CLCTX *) ckalloc(sizeof(CLCTX));
   cl_uint rc = CL_SUCCESS;

   if(CL_SUCCESS==rc) rc=clGetPlatformIDs(1,&clc->pid,NULL);
   if(CL_SUCCESS==rc) rc=clGetDeviceIDs(clc->pid,CL_DEVICE_TYPE_GPU,1,&clc->did,NULL);
   if(CL_SUCCESS==rc) clc->ctx=clCreateContext(NULL,1,&clc->did,
                                               NULL /* notify cb */, 
                                               NULL /* user data */,(cl_int *)&rc);
   if(CL_SUCCESS==rc) clc->que=clCreateCommandQueue(clc->ctx,clc->did,0,(cl_int *)&rc);
 
   if(CL_SUCCESS!=rc) {
      TclCLError(ip,rc);
      return TCL_ERROR;
   }

   Tcl_CreateObjCommand(ip,"::steenrod::cl::impl::combi",CLCombiCmd,clc,NULL);
   Tcl_CreateObjCommand(ip,"::steenrod::cl::impl::enmtest",CLEnmtestCmd,clc,NULL);

ctx_global = clc; /* hack! */

   return TCL_OK;
}

int OPCL_Init(Tcl_Interp *ip) {
   Tcl_Obj *x = Tcl_NewObj();
   cl_platform_id pid;
   cl_uint pcnt, rc;

   if( CL_SUCCESS != (rc = clGetPlatformIDs(1,&pid,&pcnt)) ) {
       TclCLError(ip,rc);
       return TCL_ERROR;
   }
   
   if (pcnt>0) {
       Tcl_ListObjAppendElement(NULL,x,Tcl_NewIntObj(0));
       AddPlatformInfo(x,pid);
   }

   Tcl_Eval(ip,"namespace eval ::steenrod::cl { namespace eval impl {} }");
   Tcl_SetVar2Ex(ip,"steenrod::cl::_info", NULL, x,0);   

   Tcl_CreateObjCommand(ip,"::steenrod::cl::impl::init",CLInitCmd,0 /* client data */,NULL);
   return TCL_OK;
}


