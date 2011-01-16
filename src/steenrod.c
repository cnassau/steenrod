/*
 * Main entry point to the Steenrod library
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

#define STEENROD_C

#include <tcl.h>
#include <string.h>
#include "tprime.h"
#include "tpoly.h"
#include "tlin.h"
#include "steenrod.h"
#include "mult.h"
#include "hmap.h"
#include "lepar.h"
#include "adlin.h"

static volatile int SIGNAL_FLAG;

#ifndef NO_SIGNALS
#include <signal.h>
void SignalHandler(int c) {
  SIGNAL_FLAG = 1;
}
#endif

#ifdef USECL
#  include "opcl.h"
#endif


int InterruptibleCmd(ClientData cd, Tcl_Interp *ip,
		     int objc, Tcl_Obj * CONST objv[]) {    
  int rc;
  void (*sigint)(int);

  if (objc != 3) {
    Tcl_WrongNumArgs(ip, 1, objv, "varname script");
    return TCL_ERROR;
  }
    
  Tcl_LinkVar(ip, Tcl_GetString(objv[1]), (char *) &SIGNAL_FLAG, TCL_LINK_INT | TCL_LINK_READ_ONLY);

  SIGNAL_FLAG = 0;
#ifndef NO_SIGNALS
  sigint = signal(SIGINT, SignalHandler);
#endif
  rc = Tcl_EvalObj(ip, objv[2]);
#ifndef NO_SIGNALS
  signal(SIGINT, sigint);
#endif

  return rc;
}




char *theprogvar; /* ckalloc'ed name of the progress variable */
int   theprogmsk; /* progress reporting granularity */

/* Our multiplication callback function. This interprets ma's client
 * data fields as follows:
 *
 *   ma->cd1 = matrixType
 *   ma->cd2 = matrix 
 *   ma->cd3 = destination enumerator
 *   ma->cd4 = error code  
 *   ma->cd5 = number of row that we're currently working on
 */

void addToMatrixCB(struct multArgs *ma, const exmo *smd) {
  matrixType *mtp = (matrixType *) ma->cd1;
  void *mat = ma->cd2;
  enumerator *dst = (enumerator *) ma->cd3;
  unsigned row = USGNFROMVPTR(ma->cd5);
  int idx;
  int rcode;
  Tcl_Interp *ip = (Tcl_Interp *) ma->TclInterp;
    
  idx = SeqnoFromEnum(dst, (exmo *) smd);
    
  if (idx < 0) { 
    ma->cd4 = (void *) FAIL;
    goto error;
  }

  /* TODO: support optional checking whether dst[idx] really equals *smd */

  rcode = mtp->addToEntry(mat, row, idx, smd->coeff, ma->prime);

  if (SUCCESS == rcode)
    return;
   
  ma->cd4 = VPTRFROMUSGN(rcode);
   
 error:
 
  if (NULL != ip) {
    char err[200];
    Tcl_Obj *aux = Tcl_NewExmoCopyObj((exmo *) smd);
    sprintf(err, 
	    "cannot account for monomial {%s}\n"
	    "    (found sequence number %d)", 
	    Tcl_GetString(aux), idx);
    Tcl_SetResult(ip, err, TCL_VOLATILE);
    DECREFCNT(aux);
  }
}

/* A MatCompTaskInfo structure is used to compute the differential of
 * a collection of polynomials and to store the result in a matrix. 
 * This abstraction is useful because we have at least two routines that 
 * carry out such a computation. */

typedef struct { 
  /* description of the result matrix */
  int srcdim;   /* number of rows to allocate */
  int currow;   /* number of row that is currently used */

  int srcIspos; /* our exmos should either be all positive or all negative */

  /* callbacks for going through the exmo; the "void *" points to ourself  */
  exmo *srcx;                  /* read-only pointer to the current exmo */
  int (*firstSource)(void *);  /* start iteration, setup srcx and currow */
  int (*nextSource)(void *);   /* go to next iteration, update src and currow */

  /* Client data, used by iterator callbacks */
  void *cd1;
    
  /* data needed to interpret the result(s) */
  momap *map;                    /* the map */
  enumerator *dst;               /* where the targets live */ 

} MatCompTaskInfo;

#define RETERR(msg)							\
  { if (NULL != ip) Tcl_SetResult(ip,msg,TCL_VOLATILE); return FAIL; }


#define PROGVARINIT							\
  if (NULL != THEPROGVAR) {						\
    Tcl_UnlinkVar(ip, THEPROGVAR);					\
    Tcl_LinkVar(ip, THEPROGVAR, (char *) &perc, TCL_LINK_DOUBLE);	\
    perc = 0;								\
  }

#define PROGVARDONE { if (NULL != THEPROGVAR) Tcl_UnlinkVar(ip, THEPROGVAR); }

#ifdef USECL

typedef struct clmemchain {
  cl_event evt;
  cl_mem gpumem;
  char *hstmem;
  int cd1,cd2,cd3;
  struct clmemchain *next;
} clmemchain;

clmemchain *clm_end(clmemchain *c) {
  while (c->next) c = c->next;
  return c;
}
void clm_append(clmemchain *c, clmemchain *new) {
  clm_end(c)->next = new;
  new->next = NULL;
}
void clm_release(clmemchain *c) {
  clmemchain *nx;
  while(c) {
    if(c->evt) clWaitForEvents(1,&(c->evt));
    if(c->gpumem) clReleaseMemObject(c->gpumem);
    c->gpumem = NULL;
    if(c->hstmem) ckfree(c->hstmem);
    c->hstmem = NULL;
    nx = c->next;
    ckfree((char *)c);
    c = nx;
  }
}

typedef struct {
  clmemchain *start, *end;
} clmemchain2;

clmemchain *clm2_append(clmemchain2 *clm, cl_event evt, cl_mem mem, char *hmem) {
  clmemchain *nw = (clmemchain *) ckalloc(sizeof(clmemchain));
  nw->gpumem = mem;
  nw->hstmem = hmem;
  nw->evt = evt;
  nw->next = NULL;
  if(NULL == clm->start) {
    clm->start = nw;
    clm->end = nw;
  } else {
    clm->end->next = nw;
    clm->end = nw;
  }
  return nw;
}
void clm2_release(clmemchain2 *clm) {
  clm_release(clm->start);
}

typedef struct {
  CLCTX *ctx;
  cl_kernel krn;
  size_t rowsize;

  clmemchain2 dgchain, srcchain;

  cl_mem srcplygpu;
  unsigned short *srcplyhst;

  cl_mem curmat;
  cl_mem seqinf;
  cl_mem semaphores;

  cl_event evt;
} cl_mult_data;

void clCopyExmo(unsigned short *wrk, const exmo *ex) {
  int j;
  for(j=0;j<NALG;j++) wrk[j] = ex->r.dat[j];
  for(;j<8;j++) wrk[j] = 0;
  wrk[15] = ex->gen & 0xffff;
  wrk[14] = ((ex->gen >> 16) & 0xffff);
  wrk[13] = ex->ext;
}

/* load polynomial into gpu-memory */
cl_int clMapPoly(CLCTX *ctx, polyType *ptp, void *ply, 
                 cl_mem *devmem, unsigned short **hstmem, size_t *numsmds, cl_event *evtptr) {
  size_t nsum = (ptp->getNumsum)(ply), size = 16*nsum*sizeof(unsigned short);
  unsigned short *hbf = (unsigned short *) ckalloc(size?size:1),*wrk;
  cl_mem res = NULL;
  int i;
  cl_int errc;
  exmo exlocal, *ex = &exlocal;
  *numsmds = nsum;

  for (i=0,wrk=hbf;i<nsum;i++,wrk+=16) {
    if(NULL != ptp->getExmoPtr)
      (ptp->getExmoPtr)(ply,&ex,i);
    else 
      (ptp->getExmo)(ply,&exlocal,i);
    clCopyExmo(wrk,ex);
  }
   
  res = clCreateBuffer(ctx->ctx,
		       CL_MEM_READ_ONLY|CL_MEM_USE_HOST_PTR,
		       size?size:1,
		       hbf,
		       &errc);

  //fprintf(stderr,"> "); for(i=0;i<size;i++) fprintf(stderr," %02x",((unsigned char *) hbf)[i]); fprintf(stderr,"\n"); 

  if(CL_SUCCESS != errc) {
    REPCLERR(errc);
  }
  *devmem = res;
  *hstmem = hbf;
  return errc;
}

int clscheduleBlock(Tcl_Interp *ip, 
		     MatCompTaskInfo *mc,
		     cl_mult_data *clmd,
		     clmemchain *curdg,
		     int dgnsums,
		     int blockstart,
		     int blockend) {
  size_t worksize[2],srcsz = blockend-blockstart;
  cl_int errc;
  cl_event evt;
  
#define BCERR(errc) do {\
    if(CL_SUCCESS!=errc) { char x[300];\
    sprintf(x,__FILE__ ", line %d: %s",__LINE__,clerrorstring(errc)); \
    Tcl_SetResult(ip,x,TCL_VOLATILE); return 0; } } while(0)

  worksize[0] = srcsz;
  worksize[1] = dgnsums;

  cl_mem smem = clCreateBuffer(clmd->ctx->ctx,
			       CL_MEM_READ_ONLY|CL_MEM_USE_HOST_PTR,
			       16*sizeof(short)*srcsz,
			       &(clmd->srcplyhst[blockstart]),
			       &errc);
  BCERR(errc);

  clmemchain *srcmem = clm2_append(&(clmd->srcchain),NULL,smem,NULL);
          
  errc = clSetKernelArg(clmd->krn,4,sizeof(int),&blockstart);
  BCERR(errc);
  errc = clSetKernelArg(clmd->krn,5,sizeof(cl_mem),&smem);
  BCERR(errc);

  //fprintf(stderr,"enqueing kernel %d x %d times\n",srcsz,dgnsums);
  errc = clEnqueueNDRangeKernel(clmd->ctx->que,clmd->krn,2,
                                 NULL,worksize,/*&locws/*/NULL,0,NULL,&evt);
  BCERR(errc);
  
  srcmem->evt = evt;
  curdg->evt = evt;

  return 1;
}

void xxxstdFetchFuncSFNoSSE(struct multArgs *ma, int coeff) {
  const exmo *sfx; int idx, i; cint c; 
  const int prime = ma->pi->prime; 
  const primeInfo * const pi = ma->pi;
  const int proext = (NULL == ma->profile) ? 0 : ma->profile->ext;
  for (idx = 0; SUCCESS == (ma->getExmoSF)(ma,SECOND_FACTOR,&sfx,idx); idx++) {
    exmo res; int hlp1;
    c = coeff;
    /* first check exterior part */
    if (ma->esum[1] != (sfx->ext & ma->esum[1])) continue;
    hlp1 = (sfx->ext ^ ma->esum[1]);
    if (0 != (hlp1 & proext)) continue; 
    if (0 != (hlp1 & ma->emsk[1])) continue;
    /* now check reduced part */
    for (i=NALG;c && i--;) {
      xint aux, aux2;
      aux  = sfx->r.dat[i] + ma->sum[0][i+1];
      if (NULL != ma->profile)
	if (0 != (aux % (ma->profile->r.dat[i]))) {
	  c = 0; 
	  break;
	}
      aux2 = ma->msk[1][i];
      if ((0 > (res.r.dat[i] = aux + aux2)) && ma->sfIsPos) {
	c = 0;
      } else {
	c = XINTMULT(c, binomp(pi, res.r.dat[i], aux), prime);
      }
    }
    if (0 == c) continue;
    res.ext = hlp1 | ma->emsk[1];
    if (0 != (1 & (SIGNFUNC(ma->emsk[1], hlp1)
		   + SIGNFUNC(ma->esum[1], hlp1))))
      c = prime - c;
    res.coeff = XINTMULT(c, sfx->coeff, prime);
    res.gen   = sfx->gen; /* should this be done in the callback function ? */
    /* invoke callback */
    (ma->stdSummandFunc)(ma, &res);
  }
}


#endif

/* MakeMatrix carries out the computation that's described in the 
 * MatCompTaskInfo argument. */

int MakeMatrix(Tcl_Interp *ip, MatCompTaskInfo *mc, exmo *profile,
               progressInfo *pinf, matrixType **mtp, void **mat) {
    
  int srcdim, dstdim;
  Tcl_Obj *dg = NULL, *theGObj;
  exmo theG; 
  polyType *dgpolyType;
  void *dgpoly;
  int dgispos, dgnumsum = 0; 
  multArgs ourMA, *ma = &ourMA;
  enumerator *dst = mc->dst;
  momap *map = mc->map;

  double perc; /* progress indicator */

#ifdef USECL
  cl_mult_data clmd;
#endif

  *mtp = NULL; *mat = NULL; /* if non-zero, caller will free this */

  /* see what kind of dg's we have to expect */
  if (mc->srcIspos) {
    dgispos = dst->ispos;
  } else {
    if (dst->ispos) {
      RETERR("map from negative to positive not possible");
    } 
    dgispos = 1;
  }

  theGObj = Tcl_NewExmoObj(&theG);
  INCREFCNT(theGObj);

  /* "theGObj"'s (exmo *) always points to the non-dynamic "theG", so don't free it when done */
#define RELEASEGOBJ { theGObj->typePtr = NULL; DECREFCNT(theGObj); }

  srcdim = mc->srcdim;
  dstdim = DimensionFromEnum(dst);

  memset(&theG, 0, sizeof(exmo));
  theG.gen = -653421; /* invalid (=highly unusual) generator id */

  initMultargs(ma, dst->pi, profile);

  ma->ffIsPos = mc->srcIspos;
  ma->sfIsPos = dgispos;

  ma->getExmoFF = &stdGetSingleExmoFunc;
  ma->getExmoSF = &stdGetExmoFunc;

  ma->fetchFuncFF = &stdFetchFuncFF;
  ma->fetchFuncSF = &stdFetchFuncSF;

  ma->cd3 = dst;
  ma->cd4 = SUCCESS;
  ma->cd5 = (void *) -1;
  ma->cd6 = (void *) 0;
  ma->TclInterp = ip;
  ma->stdSummandFunc = &addToMatrixCB;

  PROGVARINIT; 


  if(useOpenCL) {
#ifdef USECL

    clmd.srcplygpu = NULL;
    clmd.srcplyhst = NULL;
    clmd.krn = NULL;
    clmd.evt = NULL;
    clmd.seqinf = NULL;
    clmd.curmat = NULL;
    clmd.semaphores = NULL;
    clmd.dgchain.start = NULL;
    clmd.srcchain.start = NULL;

    ma->fetchFuncSF = NULL; //&clFetchFuncSF;
    ma->cd6 = &clmd;

    cl_int clerr;
    CLCTX *ctx = GetCLCtx(ip);
    clmd.ctx = ctx;

#define CHKERR(errcode) if (CL_SUCCESS != errcode)			\
      { char x[300]; snprintf(x,300,"%s (file " __FILE__ ", line %d)",clerrorstring(errcode),__LINE__); \
	Tcl_SetResult(ip,x,TCL_VOLATILE); return FAIL; }


#define DOFAIL(txt) { Tcl_SetResult(ip,txt,TCL_VOLATILE); return FAIL; }

    if(NULL == ctx->multffp) {
      ctx->multffp  = clCreateKernel(ctx->prg,"multffp",&clerr);
      CHKERR(clerr);
    } 
    clmd.krn = ctx->multffp;

    clmd.seqinf = clCreateBuffer(ctx->ctx,
                                 CL_MEM_READ_ONLY,
                                 999,
                                 NULL,
                                 &clerr);
    CHKERR(clerr);

    clmd.curmat = clCreateBuffer(ctx->ctx,
                                 CL_MEM_READ_ONLY,
                                 727,
                                 NULL,
                                 &clerr);
    CHKERR(clerr);
    
#ifndef KARG01
    clerr = clSetKernelArg(clmd.krn,0,sizeof(cl_mem),&clmd.seqinf);
    CHKERR(clerr);    
    
    clerr = clSetKernelArg(clmd.krn,1,sizeof(cl_mem),&clmd.curmat);
    CHKERR(clerr);    
#endif

#else
    Tcl_SetResult(ip,"this library has not been compiled with opencl support",TCL_STATIC);
    return FAIL;
#endif
  }

  if (useOpenCL) {
    cl_int clerr;
    cl_matrix *clmat = CreateCLMatrix(ip, srcdim, dstdim, dst->pi->prime);
    *mtp = &clMatrixType;
    *mat = clmat;
#ifndef KARG2
    if(NULL != *mat) {
      size_t nsema = srcdim*dstdim;
      int nsema2;
#define MAXSEMA 101111
      if(nsema>MAXSEMA) nsema=MAXSEMA;
      nsema2=nsema;

      clmd.semaphores = clCreateBuffer(clmd.ctx->ctx,
				       CL_MEM_READ_WRITE|CL_MEM_ALLOC_HOST_PTR,
				       sizeof(int)*nsema,
				       NULL,
				       &clerr);
      CHKERR(clerr);
      clSetKernelArg(clmd.ctx->memset0,0,sizeof(cl_mem),&(clmd.semaphores));
      clerr = clEnqueueNDRangeKernel(clmd.ctx->que,clmd.ctx->memset0,1,NULL,
				     &nsema,NULL,0,NULL,NULL);


      clerr = clSetKernelArg(clmd.krn,2,sizeof(cl_mem),&(clmat->buffer));
      CHKERR(clerr);
      clerr = clSetKernelArg(clmd.krn,3,sizeof(int),&(clmat->bytesperrow));
      CHKERR(clerr);

      clerr = clSetKernelArg(clmd.krn,7,sizeof(cl_mem),&(clmd.semaphores));
      CHKERR(clerr);
      clerr = clSetKernelArg(clmd.krn,8,sizeof(int),&nsema2);
      CHKERR(clerr);

    }
#endif
  } else {
    if (2 == dst->pi->prime) {
      *mtp = stdmatrix2;
    } else {
      *mtp = stdmatrix;
    }
   
    *mat = (*mtp)->createMatrix(srcdim, dstdim);
  }

  if (NULL == *mat) {
    RELEASEGOBJ;
    RETERR("out of memory");
  }
 
  ma->cd1 = *mtp;
  ma->cd2 = *mat;

  (*mtp)->clearMatrix(*mat);

#ifdef USECL
    cl_int clerr;
    size_t size = 16*srcdim*sizeof(unsigned short);
    unsigned short *wrk;
    int i,blockstart=0;
    clmemchain *curdg = NULL;
    size_t dgnumsmds = 0;

    if(useOpenCL) {
      clmd.srcplyhst = (unsigned short *) ckalloc(size?size:1);
      clmd.srcplygpu = NULL;
      i=0;
      wrk=clmd.srcplyhst;
    }
#endif

    if (mc->firstSource(mc)) 
      do {

	  if(useOpenCL) {
#ifdef USECL
	    clCopyExmo(wrk,mc->srcx);
	    i++;
	    wrk+=16;
#endif
	  }

	ma->cd5 = VPTRFROMUSGN(mc->currow); /* row indicator */

	if ((0 == (mc->currow & theprogmsk)) && (NULL != THEPROGVAR)) {
	  perc = mc->currow; perc /= srcdim; 
	  Tcl_UpdateLinkedVar(ip, THEPROGVAR);
	}

	/* find differential of mc->srcx'es generator */
	ma->ffdat = mc->srcx;
	ma->ffMaxLength = exmoGetRedLen(mc->srcx);
	ma->ffMaxLength = MIN(ma->ffMaxLength, NALG-2);

	if (theG.gen != mc->srcx->gen) {
	  /* new generator: need to get its differential dg */

	  theG.gen = mc->srcx->gen; 
	  dg = momapGetValPtr(map, theGObj);
                
	  if (NULL == dg) continue;

	  if (TCL_OK != Tcl_ConvertToPoly(ip, dg)) {
	    char err[200];
	    sprintf(err,"target of generator #%d not of polynomial type", 
		    theG.gen);
	    PROGVARDONE;
	    RELEASEGOBJ;
	    RETERR(err);
	  }

	  dgpolyType = polyTypeFromTclObj(dg);
	  dgpoly     = polyFromTclObj(dg);
	  dgnumsum   = PLgetNumsum(dgpolyType, dgpoly);

	  if (dgispos) {
	    if (SUCCESS != PLtest(dgpolyType, dgpoly, ISPOSITIVE)) { 
	      char err[200];
	      /* TODO: should we make sure and check each summand ? */ 
	      sprintf(err,"target of generator #%d not positive (?)", 
		      theG.gen);
	      PROGVARDONE;
	      RELEASEGOBJ;
	      RETERR(err);
	    }
	  } else {
	    if (SUCCESS != PLtest(dgpolyType, dgpoly, ISNEGATIVE)) { 
	      char err[200];
	      /* TODO: should we make sure and check each summand ? */ 
	      sprintf(err,"target of generator #%d not negative (?)", 
		      theG.gen);
	      PROGVARDONE;
	      RELEASEGOBJ;
	      RETERR(err);
	    }
	  }

	  ma->sfdat  = dgpolyType; 
	  ma->sfdat2 = dgpoly;
	  ma->sfMaxLength = PLgetMaxRedLength(dgpolyType, dgpoly);
	  ma->sfMaxLength = MIN(ma->sfMaxLength, NALG-2);

	  if(useOpenCL) {
#ifdef USECL
	    cl_mem cldg;
	    unsigned short *dghst;
	    cl_int clerr;
	    cl_event evt;

	    if(curdg) 
	      if (!clscheduleBlock(ip,mc,&clmd,curdg,
				   dgnumsmds,blockstart,mc->currow))
		ma->cd4 = VPTRFROMUSGN(FAIL);
	    blockstart = mc->currow;

	    clMapPoly(clmd.ctx,dgpolyType, dgpoly, 
		      &cldg, &dghst, &dgnumsmds, &evt);
	    
	    curdg = clm2_append(&(clmd.dgchain),NULL,cldg,(char *)dghst);
	    clerr = clSetKernelArg(clmd.krn,6,sizeof(cl_mem),&cldg);
	    CHKERR(clerr);
#endif
	  }
	}

	if (NULL == dg) continue;
     
	/* compute mc->srcx * dg */
	if (useOpenCL) {
#ifdef USECL

#endif
	} else {
	  if (mc->srcIspos)
	    workPAchain(ma);
	  else
	    workAPchain(ma);
	}

	multCount += dgnumsum;

	if (SUCCESS != USGNFROMVPTR(ma->cd4)) break;

      } while (mc->nextSource(mc));

#ifdef USECL
    if (useOpenCL) {
      if(curdg) 
	if (!clscheduleBlock(ip,mc,&clmd,curdg,
			     dgnumsmds,blockstart,mc->currow))
	  ma->cd4 = VPTRFROMUSGN(FAIL);
    }  
#endif
    
  PROGVARDONE;
  RELEASEGOBJ;
 
#ifdef USECL
  if(useOpenCL) {

    clm2_release(&(clmd.dgchain));
    clm2_release(&(clmd.srcchain));

    clFinish(clmd.ctx->que);
    if(clmd.evt) clWaitForEvents(1,&(clmd.evt));

    if(clmd.srcplygpu) clReleaseMemObject(clmd.srcplygpu);
    if(clmd.srcplyhst) ckfree((char *) clmd.srcplyhst);

    if(clmd.curmat) clReleaseMemObject(clmd.curmat);
    if(clmd.seqinf) clReleaseMemObject(clmd.seqinf);
    if(clmd.semaphores) clReleaseMemObject(clmd.semaphores);
    //if(clmd.krn) clReleaseKernel(clmd.krn);
  }
#endif
   
  if (SUCCESS != USGNFROMVPTR(ma->cd4)) {
    if (NULL != ip) {
      char err[500];
      Tcl_Obj *aux = Tcl_NewExmoCopyObj(mc->srcx);
      sprintf(err, "\nwhile computing image of {%s}", 
	      Tcl_GetString(aux));
      DECREFCNT(aux);
      Tcl_AddObjErrorInfo(ip, err, strlen(err));
    }
    return FAIL;
  }

  return SUCCESS;
}

/* --------- MakeMatrixSameSig */

int MCTfsourceEnum(void *s) {
  MatCompTaskInfo *mc = (MatCompTaskInfo *) s;
  enumerator *enu = (enumerator *) mc->cd1;
  mc->currow = 0; mc->srcx = &(enu->theex); 
  return firstRedmon(enu);
}

int MCTnsourceEnum(void *s) {
  MatCompTaskInfo *mc = (MatCompTaskInfo *) s;
  enumerator *enu = (enumerator *) mc->cd1;
  ++(mc->currow); 
  return nextRedmon(enu);
}

int MakeMatrixSameSig(Tcl_Interp *ip, enumerator *src, momap *map, enumerator *dst, 
                      progressInfo *pinf, matrixType **mtp, void **mat) {
  MatCompTaskInfo mct; 

  /* check whether src and target are compatible */
  if ((NULL == src->pi) || (src->pi != dst->pi))
    RETERR("prime mismatch");

  mct.srcIspos = src->ispos;
  mct.srcdim = DimensionFromEnum(src);

  mct.cd1 = (void *) src; 
  mct.srcx = &(src->theex);
  mct.firstSource = MCTfsourceEnum;
  mct.nextSource  = MCTnsourceEnum;

  mct.dst = dst; 
  mct.map = map;

  return MakeMatrix(ip, &mct, &(src->profile), pinf, mtp, mat);
}

int TMakeMatrixSameSig(ClientData cd, Tcl_Interp *ip,
                       int objc, Tcl_Obj * CONST objv[]) {

  enumerator *src, *dst;
  momap *map; 
  progressInfo info, *infoptr;
  matrixType *mtp;
  void *mat;

  if ((objc<4) || (objc>6)) {
    Tcl_WrongNumArgs(ip, 1, objv, 
		     "<enumerator> <monomap> <enumerator> ?<varname>? ?<int>?");
    return TCL_ERROR;
  }
    
  info.ip = ip; 
  info.progvar = NULL;
  info.pmsk = 0;
  infoptr = NULL;

  if (objc > 4) { 
    info.progvar = Tcl_GetString(objv[4]);
    infoptr = &info;
  }

  if (objc > 5) 
    if (TCL_OK != Tcl_GetIntFromObj(ip, objv[5], &info.pmsk))
      return TCL_ERROR;

  if (NULL == (src = Tcl_EnumFromObj(ip, objv[1]))) {
    Tcl_AppendResult(ip, " (argument #1)", NULL);
    return TCL_ERROR;
  }

  if (NULL == (map = Tcl_MomapFromObj(ip, objv[2]))) {
    Tcl_AppendResult(ip, " (argument #2)", NULL);
    return TCL_ERROR;
  }

  if (NULL == (dst = Tcl_EnumFromObj(ip, objv[3]))) {
    Tcl_AppendResult(ip, " (argument #3)", NULL);
    return TCL_ERROR;
  }

  if (SUCCESS != MakeMatrixSameSig(ip, src, map, dst, infoptr, &mtp, &mat)) {
    if (NULL != mat) mtp->destroyMatrix(mat);
    return TCL_ERROR;
  }
  
  Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mtp, mat));
  return TCL_OK;
}


/* --------- MakeImages */

/* A "PlistCtrlStruct" is used to iterate through the monomials of a list
 * of polynomials. This is a utility for the "MakeImages" function below. */

typedef struct {
  polyType **pt;
  void     **pdat;
  int npoly;
  int idx, aux, pcnt;
  exmo xm;
  int ispos;
} PlistCtrlStruct; 

int MCTnpcs(void *s) {
  MatCompTaskInfo *mct = (MatCompTaskInfo *) s;
  PlistCtrlStruct *pcs = (PlistCtrlStruct *) mct->cd1;
  ++(pcs->idx);
  do {
    if (pcs->idx < pcs->aux) {
      polyType *pt = pcs->pt[pcs->pcnt];
      void *pdat = pcs->pdat[pcs->pcnt];
      if (SUCCESS != PLgetExmo(pt, pdat, &(pcs->xm), pcs->idx))
	return 0;
      return 1;
    }
    pcs->idx = 0; 
    if (++(pcs->pcnt) >= pcs->npoly) break;
    pcs->aux = PLgetNumsum(pcs->pt[pcs->pcnt], pcs->pdat[pcs->pcnt]);  
    ++(mct->currow);
  } while (1);
  return 0;
}

int MCTfpcs(void *s) {
  MatCompTaskInfo *mct = (MatCompTaskInfo *) s;
  PlistCtrlStruct *pcs = (PlistCtrlStruct *) mct->cd1;
  if (0 == pcs->npoly) return 0;
  pcs->idx = pcs->pcnt = 0; 
  pcs->aux = PLgetNumsum(pcs->pt[0], pcs->pdat[0]);
  mct->currow = 0;
  if (0 == pcs->aux) return MCTnpcs(s);
  if (SUCCESS != PLgetExmo(pcs->pt[0], pcs->pdat[0], &(pcs->xm), 0))
    return 0;
  return 1;
}

void destroyPCS(PlistCtrlStruct *pcs) {
  if (NULL != pcs->pt) freex(pcs->pt);
  if (NULL != pcs->pdat) freex(pcs->pdat);
}

#define THROWUP { destroyPCS(pcs); return FAILMEM; }

int makePCS(PlistCtrlStruct *pcs, Tcl_Obj *plist) {
  int i, ispos, isneg, obc; Tcl_Obj **obv;
    
  pcs->pt = NULL; pcs->pdat = NULL;

  if (TCL_OK != Tcl_ListObjGetElements(NULL, plist, &obc, &obv))
    return FAIL;

  pcs->npoly = obc;
  pcs->ispos = 1;
  pcs->pt    = mallox(obc * sizeof(polyType *));
  pcs->pdat  = mallox(obc * sizeof(void *));
    
  if ((NULL == pcs->pt) || (NULL == pcs->pdat)) 
    THROWUP;

  ispos = isneg = 1;
  for (i=0; i<obc; i++) 
    if (TCL_OK == Tcl_ConvertToPoly(NULL, obv[i])) {
      pcs->pt[i]   = polyTypeFromTclObj(obv[i]);
      pcs->pdat[i] = polyFromTclObj(obv[i]);
      ispos = ispos && 
	(SUCCESS == PLtest(pcs->pt[i], pcs->pdat[i], ISPOSITIVE)); 
      isneg = isneg && 
	(SUCCESS == PLtest(pcs->pt[i], pcs->pdat[i], ISNEGATIVE));
    } else THROWUP;
        
  if (!ispos && !isneg) 
    THROWUP;

  pcs->ispos = ispos;

  return SUCCESS;
}

int MakeImages(Tcl_Interp *ip, Tcl_Obj *plist, momap *map, enumerator *dst,
               progressInfo *pinf, matrixType **mtp, void **mat) {
  MatCompTaskInfo mct; 
  int rcode;
  PlistCtrlStruct pcs;
    
  if (SUCCESS != makePCS(&pcs, plist))
    return FAIL;

  mct.srcIspos = pcs.ispos;
  mct.srcdim = pcs.npoly;

  mct.cd1 = &pcs; 
  mct.srcx = &(pcs.xm);
  mct.firstSource = MCTfpcs;
  mct.nextSource  = MCTnpcs;

  mct.dst = dst; 
  mct.map = map;

  rcode = MakeMatrix(ip, &mct, &(dst->profile), pinf, mtp, mat);

  destroyPCS(&pcs);

  return rcode;
}

int TMakeImages(ClientData cd, Tcl_Interp *ip,
                int objc, Tcl_Obj * CONST objv[]) {

  enumerator *dst;
  momap *map; 
  progressInfo info, *infoptr;
  matrixType *mtp = NULL;
  void *mat = NULL;
  int i, obc; Tcl_Obj **obv;
    
  if ((objc<4) || (objc>6)) {
    Tcl_WrongNumArgs(ip, 1, objv, 
		     "<monomap> <enumerator> <list of polynomials>"
		     " ?<varname>? ?<int>?");
    return TCL_ERROR;
  }
    
  info.ip = ip; 
  info.progvar = NULL;
  info.pmsk = 0;
  infoptr = NULL;

  if (objc > 4) { 
    info.progvar = Tcl_GetString(objv[4]);
    infoptr = &info;
  }

  if (objc > 5) 
    if (TCL_OK != Tcl_GetIntFromObj(ip, objv[5], &info.pmsk))
      return TCL_ERROR;

  if (NULL == (map = Tcl_MomapFromObj(ip, objv[1]))) {
    Tcl_AppendResult(ip, " (argument #1)", NULL);
    return TCL_ERROR;
  }

  if (NULL == (dst = Tcl_EnumFromObj(ip, objv[2]))) {
    Tcl_AppendResult(ip, " (argument #2)", NULL);
    return TCL_ERROR;
  }

  /* check that objv[3] is a list of polynomials */
    
  if (TCL_OK != Tcl_ListObjGetElements(ip, objv[3], &obc, &obv)) {
    Tcl_AppendResult(ip, " (expected list of polynomials)", NULL);
    return TCL_ERROR;
  }

  for (i=0;i<obc;i++) 
    if (TCL_OK != Tcl_ConvertToPoly(ip, obv[i])) 
      RETERR("argument 3 should be a list of polynomials");
       
  if (SUCCESS != MakeImages(ip, objv[3], map, dst, infoptr, &mtp, &mat)) {
    if (NULL != mat) mtp->destroyMatrix(mat);
    return TCL_ERROR;
  }
  
  Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mtp, mat));

  return TCL_OK;
}

/* Convert input into a Tcl_ByteArray object. Required
 * for sqlite if value should be treated as blob */

int MakeBinaryCmd(ClientData cd, Tcl_Interp *ip,
                  int objc, Tcl_Obj * CONST objv[]) {
  Tcl_ObjType *bin = (Tcl_ObjType *) cd;
  Tcl_Obj *res = objv[1];

  if (objc != 2) {
    Tcl_WrongNumArgs(ip, 1, objv, "stringValue");
    return TCL_ERROR;
  }

  if (res->typePtr != bin) {
    int len; 
    unsigned char *bytes = Tcl_GetByteArrayFromObj(res,&len);
    res = Tcl_NewByteArrayObj(bytes,len);
  }

  Tcl_SetObjResult(ip, res);
  return TCL_OK;
}

/* Removing the string representation can sometimes help to save memory: */

#define STEALCOMMAND "StealStringRep"
int StealStringRep(ClientData cd, Tcl_Interp *ip,
                   int objc, Tcl_Obj * CONST objv[]) {
  Tcl_Obj *obj;

  if (objc != 2) 
    RETERR("usage: " STEALCOMMAND " <argument>");
    
  obj = objv[1];

  if (NULL != obj->typePtr) 
    if (NULL != obj->typePtr->updateStringProc)
      Tcl_InvalidateStringRep(obj);

  return TCL_OK;
}

int GetRefCount(ClientData cd, Tcl_Interp *ip,
                int objc, Tcl_Obj * CONST objv[]) {
  if (objc != 2) {
    Tcl_SetResult(ip, "usage: ", TCL_STATIC);
    Tcl_AppendResult(ip, Tcl_GetString(objv[0]), " <argument>", NULL);
    return TCL_ERROR;
  }
    
  Tcl_SetObjResult(ip, Tcl_NewIntObj(objv[1]->refCount));

  return TCL_OK;
}

int VersionCmd(ClientData cd, Tcl_Interp *ip,
               int objc, Tcl_Obj * CONST objv[]) {
  Tcl_SetResult(ip, PACKAGE_NAME "-" PACKAGE_VERSION 
#ifdef USESSE2
		"-sse2"
#endif
#ifdef USECL
		"-opencl"
#endif
		,TCL_STATIC);
  return TCL_OK;
}

EXTERN int Steenrod_Init(Tcl_Interp *ip) {

  Tcl_InitStubs(ip, "8.0", 0);

  objCount = 0;
  useOpenCL = 0;

#ifdef USESSE2
  {
    /* check whether SSE2 instructions seem to work */
    int sse2ok = 1;
    __m128i var = _mm_set_epi16(7,6,5,4,3,2,1,0);
#define TESTENT(i) { if (i != _mm_extract_epi16(var,i)) sse2ok = 0; }
    TESTENT(0);
    TESTENT(1);
    TESTENT(2);
    TESTENT(3);
    TESTENT(4);
    TESTENT(5);
    TESTENT(6);
    TESTENT(7);
    if(!sse2ok) {
      Tcl_SetResult(ip, "library requires sse2"
		    " capable processor"
		    " (recompile without -DUSESSE2)", 
		    TCL_STATIC);
      return TCL_ERROR;
    }
  }
#endif

  Tptr_Init(ip);
  Tprime_Init(ip);
  Tpoly_Init(ip);
  Momap_Init(ip);
  Tenum_Init(ip);
  Tlin_Init(ip);
  Hmap_Init(ip);
#if 0
  Lepar_Init(ip);
#endif

  Tcl_CreateObjCommand(ip, POLYNSP "ComputeMatrix",
		       TMakeMatrixSameSig, (ClientData) 0, NULL);

  Tcl_CreateObjCommand(ip, POLYNSP "ComputeImage",
		       TMakeImages, (ClientData) 0, NULL);

  Tcl_CreateObjCommand(ip, STEALCOMMAND,
		       StealStringRep, (ClientData) 0, NULL);

  Tcl_CreateObjCommand(ip, POLYNSP "_refcount",
		       GetRefCount, (ClientData) 0, NULL);

  Tcl_CreateObjCommand(ip, POLYNSP "Version",
		       VersionCmd, (ClientData) 0, NULL);

  Tcl_CreateObjCommand(ip, POLYNSP "interruptible",
		       InterruptibleCmd, (ClientData) 0, NULL);

  {
    Tcl_Obj * bin = Tcl_NewByteArrayObj((unsigned char *) &bin,1);
    Tcl_CreateObjCommand(ip, POLYNSP "tcl_binary",
			 MakeBinaryCmd, (ClientData) bin->typePtr, NULL);
    Tcl_DecrRefCount(bin);
  }

#ifdef USECL
  Tcl_UnlinkVar(ip, POLYNSP "useOpenCL");
  Tcl_LinkVar(ip, POLYNSP "useOpenCL", (char *) &useOpenCL, TCL_LINK_INT);
  OPCL_Init(ip);
#endif

  /* create links for progress reporting */
  Tcl_UnlinkVar(ip, POLYNSP "_progvarname");
  Tcl_UnlinkVar(ip, POLYNSP "_progsteps");

  Tcl_LinkVar(ip, POLYNSP "_progvarname", (char *) &theprogvar, TCL_LINK_STRING);
  Tcl_LinkVar(ip, POLYNSP "_progsteps", (char *) &theprogmsk, TCL_LINK_INT);

  theprogvar = ckalloc(1); *theprogvar = 0; theprogmsk = 0x1;

  Tcl_UnlinkVar(ip, POLYNSP "_objCount");
  Tcl_LinkVar(ip, POLYNSP "_objCount", (char *) &objCount, TCL_LINK_INT | TCL_LINK_READ_ONLY);

  Tcl_Eval(ip, "namespace eval " POLYNSP 
	   " { namespace export -clear \\[a-z\\]* }");

  Tcl_PkgProvide(ip, PACKAGE_NAME, PACKAGE_VERSION);

  return TCL_OK;
}
