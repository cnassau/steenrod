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
#include <string.h>
#include "tenum.h"


#define MATLOG(res,txt) if (0) {					\
    fprintf(stderr,"%s cl_matrix %p (%d,%d), device=%p, host=%p\n",	\
	    txt,res,res->rows,res->cols,res->buffer,res->hostbuf); }

void DestroyCLMatrix(cl_matrix *res) {
  MATLOG(res,"destroying");
  if(res->hostbuf) free(res->hostbuf);
  if(res->buffer) clReleaseMemObject(res->buffer);    
  ckfree((char *)res);
}

cl_matrix *CreateCLMatrix(Tcl_Interp *ip, int rows, int cols, int prime) {
  cl_int errc;
  CLCTX *ctx = GetCLCtx(ip);
  cl_matrix *res = (cl_matrix *) ckalloc(sizeof(cl_matrix));
  if(NULL == res) return NULL;
  res->rows = rows;
  res->cols = cols;
  res->prime = prime;
  res->bytesperrow = ((cols+15)/16)*16;
  res->size = res->bytesperrow * res->rows;
  if(0 == res->size) res->size=1;
  res->size = 16*((res->size + 15)/16); 
  res->ctx = ctx;
  res->hostbuf = NULL;
  res->buffer = clCreateBuffer(ctx->ctx,
			       CL_MEM_READ_WRITE,
			       res->size,
			       NULL,
			       &errc);
  // fprintf(stderr,"size=%ld\n",res->size);
  if(CL_SUCCESS != errc) {
    res->buffer = NULL;
    fprintf(stderr,"could not allocate opencl matrix of size %d x %d: %s\n",
	    rows,cols,clerrorstring(errc));
    DestroyCLMatrix(res);
    return NULL;
  }        
  MATLOG(res,"created");
  return res;
}

int clMapMatrix(cl_matrix *mat) {
  if(NULL == mat->hostbuf) {
    mat->hostbuf = malloc(mat->size);
  }
  clEnqueueReadBuffer(mat->ctx->que,mat->buffer,
		      CL_TRUE,0,mat->size,mat->hostbuf,0,NULL,NULL);
//{int k;fprintf(stderr,"mapped memory: "); for(k=0;k<mat->size;k++) fprintf(stderr," %d",mat->hostbuf[k]); fprintf(stderr,"\n");}
  MATLOG(mat,"mapped");
  return SUCCESS;
}

int  (clGetEntry)(void *mat, int row, int col, int *val) {
  cl_matrix *m = (cl_matrix *) mat;
  if(NULL==m->hostbuf) clMapMatrix(m);
  *val = m->hostbuf[m->bytesperrow*row+col];
  //fprintf(stderr,"(%d,%d)=%d\n",row,col,*val);
  MATLOG(m,"getentry");
  return SUCCESS;
}
int  (clSetEntry)(void *mat, int row, int col, int val) {
  cl_matrix *m = (cl_matrix *) mat;
  if(NULL==m->hostbuf) clMapMatrix(m);
  m->hostbuf[m->bytesperrow*row+col] = val;
  return SUCCESS;
};
int  (clAddEntry)(void *mat, int row, int col, int val, int mod) {
  cl_matrix *m = (cl_matrix *) mat;
  if(NULL==m->hostbuf) clMapMatrix(m);
  m->hostbuf[m->bytesperrow*row+col] += val;
  return SUCCESS;
};
void (clGetDimensions)(void *mat, int *row, int *col) {
  cl_matrix *m = (cl_matrix *) mat;
  *row = m->rows;
  *col = m->cols;
  MATLOG(m,"get dimensions");
};
void *(clCreateCopy)(void *mat) {
  cl_matrix *m = (cl_matrix *) mat, 
    *res=(cl_matrix *) ckalloc(sizeof(cl_matrix));
  if(NULL==m->hostbuf) clMapMatrix(m);
  memcpy(res,m,sizeof(cl_matrix));
  res->buffer = NULL;
  res->hostbuf = malloc(res->size);
  memcpy(res->hostbuf,m->hostbuf,res->size);
  MATLOG(m,"copy: in ");
  MATLOG(res,"copy: out");
  return res;
};
void (clDestroyMatrix)(void *mat) {
  DestroyCLMatrix((cl_matrix *) mat);
};
void clUnmapMatrix(cl_matrix * m) {
  MATLOG(m,"unmapping");
  if(NULL != m->hostbuf) {
    free(m->hostbuf); 
    m->hostbuf = NULL;
  };
}
#define LOGCLERR(errc) \
  {if(CL_SUCCESS!=errc) fprintf(stderr,__FILE__ ", %d: opencl error: %s", __LINE__,clerrorstring(errc)); }

void (clClearMatrix)(void *mat) {
  cl_matrix *m = (cl_matrix *) mat;
  cl_int clerr;
  clUnmapMatrix(m);
  CLCTX *ctx=m->ctx;
  cl_kernel krn = ctx->memset0;
  clerr = clSetKernelArg(krn,0,sizeof(cl_mem),&(m->buffer));
  LOGCLERR(clerr);
  size_t globws = m->size/16;
  MATLOG(m,"clearing");
  //fprintf(stderr,"enqueing kernel %p for %d threads and %ld bytes (%d x %d)\n",krn,globws,m->size,m->rows,m->cols);
  clerr = clEnqueueNDRangeKernel(ctx->que,krn,1,NULL,
				 &globws,/*&locws/*/NULL,0,NULL,NULL);
  clFlush(ctx->que);
  LOGCLERR(clerr);
};
void (clUnitMatrix)(void *mat) {
};
int  (clReduce)(void *mat, int prime) {
  return SUCCESS;
};
int  (clIsZero)(void *mat) {
  return SUCCESS;
};
void *(clShrink)(void *mat, int *idx, int num) {
  cl_matrix *m = (cl_matrix *) mat;
  MATLOG(m,"shrinking");
  return NULL;
};
int  (clAdd)(void *v1, void *v2, int scale, int mod) {
};
void *(clOrtho)(primeInfo *pi, void *inp, void *urb, progressInfo *prg) {
  return NULL;
};
void *(clLift)(primeInfo *pi, void *inp, void *lft, progressInfo *prg) {
  return NULL;
};
void (clQuot)(primeInfo *pi, void *ker, void *im, progressInfo *prg) {
};


matrixType clMatrixType = {
  .name = "opencl matrix",
  .getEntry = clGetEntry,
  .setEntry = clSetEntry,
  .addToEntry = clAddEntry,
  .getDimensions = clGetDimensions,
  .createMatrix = NULL,
  .createCopy = clCreateCopy,
  .destroyMatrix = clDestroyMatrix,
  .clearMatrix = clClearMatrix,
  .unitMatrix = clUnitMatrix,
  .reduce = clReduce,
  .iszero = clIsZero,
  .shrinkRows = clShrink,
  .add = clAdd,
  .orthoFunc = clOrtho,
  .liftFunc = clLift,
  .quotFunc = clQuot
};

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

#define REPPAR(par)							\
  { if(CL_SUCCESS == clGetPlatformInfo(pid,par,1000,pval,NULL)) {	\
      Tcl_ListObjAppendElement(NULL,p,Tcl_NewStringObj(#par,-1));	\
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


    clc->memset0 = clCreateKernel(clc->prg,"memset0",&err);
    ckerr;


    return TCL_OK;
  }


  return TCL_ERROR;
}

void DeleteCTX(void *cd) {
   CLCTX *ctx = GetCLCtx((Tcl_Interp *) cd);
   if(ctx->memset0) clReleaseKernel(ctx->memset0);
   if(ctx->multffp) clReleaseKernel(ctx->multffp);
   clReleaseCommandQueue(ctx->que);
   clReleaseProgram(ctx->prg);
   clReleaseContext(ctx->ctx);
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

  Tcl_CreateObjCommand(ip,"::steenrod::cl::impl::init",CLInitCmd,ip /* client data */,DeleteCTX);
  return TCL_OK;
}


