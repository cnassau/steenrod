/*
 * Tcl interface to the enumerator structure
 *
 * Copyright (C) 2004-2019 Christian Nassau <nassau@nullhomotopie.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <tcl.h>
#include "setresult.h"
#include <string.h>
#include "tprime.h"
#include "tpoly.h"
#include "tlin.h"
#include "linwrp.h"
#include "poly.h"
#include "tenum.h"
#include "enum.h"

#if USEOPENCL
#  include "opencl.h"
#endif

#define FLOG 0
#define DOFLOG(msg) { if (FLOG) fprintf(stderr, msg "\n"); }

/* "needsUpdate" is used to indicate that configuration parameters need to
 * be recreated. We cannot use NULL, since that indicates the empty default. */
char nup[] = "parameter needs update";
#define needsUpdate ((Tcl_Obj *) nup)

/* "defaultParameter" is used to indicate that an empty value was given for
 * a configuration parameter. We cannot use NULL, since that indicates that
 * no parameter was given. */
char dfp[] = "default parameter";
#define defaultParameter ((Tcl_Obj *) dfp)

#define STRLEN(x) (sizeof(x)-1)  /* use only with constant strings */

#define RETERR(errmsg) \
{ if (NULL != ip) Tcl_SetResult(ip, (char*)errmsg, TCL_VOLATILE) ; return TCL_ERROR; }

/* A tclEnumerator is an enumerator plus configuration objects.
 * The configuration flags indicate whether the option has been changed. */

typedef struct {
    /* Configuration values & flags. */
    Tcl_Obj *prime, *alg, *pro, *sig, *ideg, *edeg, *hdeg, *genlist, *ispos;
    int     cprime, calg, cpro, csig, cideg, cedeg, chdeg, cgenlist, cispos;

    /* new genlist */
    int *gl, gllength;

    /* The actual data. */
    enumerator *enm;
#if USEOPENCL
    clenum cl;
#endif
} tclEnum;

#define TRYFREEOBJ(obj) \
{ if ((NULL != (obj)) && (needsUpdate != (obj) && (defaultParameter != (obj)))) \
  { DECREFCNT(obj); obj = NULL; }; }

#define FREERESRET { freex(res); return NULL; }
#define FREERESRETERR(msg) \
{ if (NULL != ip) Tcl_SetResult(ip,msg,TCL_VOLATILE); FREERESRET; }

char wfem[] = "wrong generator format, expected {gen-id int-deg ext-deg hom-deg}";

int *getGenList(Tcl_Interp *ip, Tcl_Obj *obj, int *length) {
    int objc, *res, *wrk;
    Tcl_Obj **objv;

    if (TCL_OK != Tcl_ListObjGetElements(ip, obj, &objc, &objv))
        return NULL;

    *length = objc;

    if (NULL == (res = (int *) callox((0+objc) * 4, sizeof(int))))
        return NULL;

#if 0
    printf("genlist = %s\n",Tcl_GetString(obj));
    Tcl_Eval(ip, "flush stdout");
#endif

    for (wrk=res; objc--; (objv)++, wrk+=4) {
        int objc2, *aux;
        Tcl_Obj **objv2;

        if (TCL_OK != Tcl_ListObjGetElements(ip, *objv, &objc2, &objv2))
            FREERESRET;

        if ((objc2<1) || (objc2>4)) FREERESRETERR(wfem);

        for (aux=wrk; objc2--; ++(objv2),aux++)
            if (TCL_OK != Tcl_GetIntFromObj(ip, *objv2, aux))
                 FREERESRETERR(wfem);
    }

    return res;
}

static const char *typeNames[] = { "positive", "negative", NULL };

int checkForType(Tcl_Interp *ip, Tcl_Obj *obj, int *ispos) {
    int index;
    if (NULL == obj) return TCL_ERROR;
    if (TCL_OK != Tcl_GetIndexFromObj(ip, obj, typeNames, "type", 0, &index))
        return TCL_ERROR;

    /* we prefer to report the type option as "positive / negative", so
     * positive will get index 0, and we need to reverse this here: */
    if (NULL != ispos) *ispos = !index;

    return TCL_OK;
}

/* Here we try to activate the configured values. */

int Tcl_EnumSetValues(ClientData cd, Tcl_Interp *ip) {
    tclEnum *te = (tclEnum *) cd;
    primeInfo *pi;
    exmo *alg, *pro, *sig;
    int ispos = 1; /* default: positive */

#define TRYGETEXMO(ex,obj)                          \
if (NULL != (obj)) {                                \
   if (TCL_OK != Tcl_ConvertToExmo(ip, (obj)))      \
       return TCL_ERROR;                            \
   ex = exmoFromTclObj(obj);                        \
} else ex = (exmo *) NULL;

    DOFLOG("Entered Tcl_EnumSetValues");

    if (te->cprime || te->calg || te->cpro || te->cispos) {
        if (NULL == te->prime) RETERR("prime not given");
        if (TCL_OK != Tcl_GetPrimeInfo(ip, te->prime, &pi))
            return TCL_ERROR;

        TRYGETEXMO(alg,te->alg);
        TRYGETEXMO(pro,te->pro);

        if (TCL_OK != checkForType(ip, te->ispos, &ispos))
            return TCL_ERROR;

        enmSetBasics(te->enm, pi, alg, pro, ispos);
        te->cprime = te->calg = te->cpro = te->cispos = 0;
    }

    if (te->csig) {
        TRYGETEXMO(sig,te->sig);

        enmSetSignature(te->enm, sig);
        te->csig = 0;
    }

#define TRYGETINT(obj,var)                               \
if (NULL != (obj))                                       \
    if (TCL_OK != Tcl_GetIntFromObj(ip, (obj), &(var)))  \
        return TCL_ERROR;

    if (te->cideg || te->cedeg || te->chdeg) {
        int nideg = te->enm->ideg,
            nedeg = te->enm->edeg,
            nhdeg = te->enm->hdeg;

        TRYGETINT(te->ideg, nideg);
        TRYGETINT(te->edeg, nedeg);
        TRYGETINT(te->hdeg, nhdeg);

        enmSetTridegree(te->enm, nideg, nedeg, nhdeg);
        te->cideg = te->cedeg = te->chdeg = 0;
    }

    if (te->cgenlist) {
        if (SUCCESS != enmSetGenlist(te->enm, te->gl, (te->gllength) / 1))
            RETERR("duplicate generator id");
        te->gl = NULL; te->gllength = 0;
        te->cgenlist = 0;
    }

    DOFLOG("Leaving Tcl_EnumSetValues");

    return TCL_OK;
}

static Tcl_Obj *ourPosObj; /* always holds the value "positive" */
static Tcl_Obj *ourNegObj; /* always holds the value "negative" */

typedef enum  { PRIME, ALGEBRA, PROFILE, SIGNATURE,
                TYPE, IDEG, EDEG, HDEG, GENLIST } enumoptcode;

static const char *optNames[] = { "-prime", "-algebra", "-profile", "-signature",
                                  "-type", "-ideg", "-edeg", "-hdeg", "-genlist",
                                  (char *) NULL };

static enumoptcode optmap[] = { PRIME, ALGEBRA, PROFILE, SIGNATURE,
                                TYPE, IDEG, EDEG, HDEG, GENLIST };

/* note that this command differs from the others: it assumes that option value
 * pairs start at objv[0]. */
int Tcl_EnumConfigureCmd(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd, aux;

    int result;
    int index;

    primeInfo *pi;

    DOFLOG("Entered Tcl_EnumConfigureCmd");

    if (0 == objc) {
        Tcl_Obj *co[2], *res = Tcl_NewListObj(0, NULL);

        /* describe current configuration status */

        if (needsUpdate == te->sig) {
            te->sig = Tcl_NewExmoCopyObj(&(te->enm->signature));
            INCREFCNT(te->sig);
        }

        if (needsUpdate == te->genlist) {
            /* TODO: recreate genlist from enum if that has been changed
             * by a future addgen cmd */
        }

#define APPENDOPT(name, obj) {                                          \
co[0] = Tcl_NewStringObj(name,STRLEN(name));                            \
co[1] = (NULL != (obj)) ? (obj) : Tcl_NewObj();                         \
if (TCL_OK != Tcl_ListObjAppendElement(ip, res, Tcl_NewListObj(2,co)))  \
   { DECREFCNT(res); return TCL_ERROR; };                        \
}

        APPENDOPT("-prime", te->prime);
        APPENDOPT("-algebra", te->alg);
        APPENDOPT("-profile", te->pro);
        APPENDOPT("-signature", te->sig);
        APPENDOPT("-type", te->ispos);
        APPENDOPT("-ideg", te->ideg);
        APPENDOPT("-edeg", te->edeg);
        APPENDOPT("-hdeg", te->hdeg);
        APPENDOPT("-genlist", te->genlist);
        Tcl_SetObjResult(ip, res);
        return TCL_OK;
    }

    /* aux keeps the new config values, before they have been validated */
    memset(&aux, 0, sizeof(tclEnum));

    if (0 != (objc & 1)) RETERR("option/value pairs excpected");

    for (; objc; objc-=2,objv+=2) {
        Tcl_Obj *value; int length;
        result = Tcl_GetIndexFromObj(ip, objv[0], optNames, "option", 0, &index);
        if (TCL_OK != result) return result;

        value = objv[1];

        /* ISDEFAULTARG changes "value" to "defaultParameter" if objv[1] is
         * empty. Since this implies getting the string representation
         * of objv[1] we try to avoid this as much as possible. */

#define ISDEFAULTARG(var) \
(value = objv[1], Tcl_GetStringFromObj(value, &length), \
 (var = length ? var : (value = defaultParameter)), (0==length))

        switch (optmap[index]) {
            case PRIME:
                aux.prime = value;
                if (TCL_OK == Tcl_GetPrimeInfo(ip, value, &pi)) break;
                if (ISDEFAULTARG(aux.prime)) break;
                return TCL_ERROR;
            case ALGEBRA:
                aux.alg = value;
                if (TCL_OK == Tcl_ConvertToExmo(ip, value)) break;
                if (ISDEFAULTARG(aux.alg)) break;
                return TCL_ERROR;
            case PROFILE:
                aux.pro = value;
                if (TCL_OK == Tcl_ConvertToExmo(ip, value)) break;
                if (ISDEFAULTARG(aux.pro)) break;
                return TCL_ERROR;
            case SIGNATURE:
                aux.sig = value;
                if (TCL_OK == Tcl_ConvertToExmo(ip, value)) break;
                if (ISDEFAULTARG(aux.sig)) break;
                return TCL_ERROR;
            case TYPE:
                if (TCL_OK != checkForType(ip, value, &result))
                    return TCL_ERROR;
                aux.ispos = result ? ourPosObj : ourNegObj;
                break;
            case IDEG:
                aux.ideg = value;
                if (TCL_OK == Tcl_GetIntFromObj(ip, value, &result)) break;
                if (ISDEFAULTARG(aux.ideg)) break;
                return TCL_ERROR;
            case EDEG:
                aux.edeg = value;
                if (TCL_OK == Tcl_GetIntFromObj(ip, value, &result)) break;
                if (ISDEFAULTARG(aux.edeg)) break;
                return TCL_ERROR;
            case HDEG:
                aux.hdeg = value;
                if (TCL_OK == Tcl_GetIntFromObj(ip, value, &result)) break;
                if (ISDEFAULTARG(aux.hdeg)) break;
                return TCL_ERROR;
            case GENLIST:
                if (objv[1] == te->genlist) break;
                if (NULL != te->gl) freex(te->gl);
                if (NULL == (te->gl = getGenList(ip, objv[1], &(te->gllength))))
                    return TCL_ERROR;
                aux.genlist = objv[1];
                break;
        }
    }

    /* now check which options are new */
    if (NULL != aux.prime) {
        if (defaultParameter == aux.prime) {
            TRYFREEOBJ(te->prime);
            te->prime = NULL;
            te->cprime = 1;
        } else {
            primeInfo *pi2 = NULL;
            if (NULL != te->prime) Tcl_GetPrimeInfo(ip, te->prime, &pi2);
            if ((NULL == te->prime) || (pi2 != pi)) {
                TRYFREEOBJ(te->prime);
                te->prime = aux.prime;
                INCREFCNT(te->prime);
                te->cprime = 1;
            }
        }
    }

#define SETOPT(old,new,flag)                                           \
if (NULL != (new)) {                                                   \
   if (defaultParameter != (new)) {                                    \
      TRYFREEOBJ(old); (old) = (new); INCREFCNT(old); flag = 1; \
   } else { TRYFREEOBJ(old); (old) = NULL; flag = 1; } }

    SETOPT(te->alg, aux.alg, te->calg);
    SETOPT(te->pro, aux.pro, te->cpro);
    SETOPT(te->sig, aux.sig, te->csig);
    SETOPT(te->ispos, aux.ispos, te->cispos);
    SETOPT(te->ideg, aux.ideg, te->cideg);
    SETOPT(te->edeg, aux.edeg, te->cedeg);
    SETOPT(te->hdeg, aux.hdeg, te->chdeg);

    if (NULL != aux.genlist) {
        TRYFREEOBJ(te->genlist);
        te->genlist = aux.genlist;
        aux.genlist = NULL; te->cgenlist = 1;
        INCREFCNT(te->genlist);
    }

    DOFLOG("Leaving Tcl_EnumConfigureCmd");

    Tcl_ResetResult(ip);
    return TCL_OK;
}

int Tcl_EnumCgetCmd(ClientData cd, Tcl_Interp *ip,
                    int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd;
    int result, index;
    Tcl_Obj *auxobj;

    if (objc != 3) {
        Tcl_WrongNumArgs(ip, 2, objv, "option");
        return TCL_ERROR;
    }

#define SETRESRET(obj) \
{ Tcl_SetObjResult(ip,(NULL != (obj)) ? obj : Tcl_NewObj()); return TCL_OK; }

    result = Tcl_GetIndexFromObj(ip, objv[2], optNames, "option", 0, &index);
    if (TCL_OK != result) return result;
    switch (optmap[index]) {
        case PRIME:     SETRESRET(te->prime);
        case ALGEBRA:   SETRESRET(te->alg);
        case PROFILE:   SETRESRET(te->pro);
        case SIGNATURE:
            if (needsUpdate == te->sig) {
                auxobj = Tcl_NewExmoCopyObj(&(te->enm->signature));
                SETRESRET(auxobj);
            }
            SETRESRET(te->sig);
        case TYPE:      SETRESRET(te->ispos);
        case IDEG:      SETRESRET(te->ideg);
        case EDEG:      SETRESRET(te->edeg);
        case HDEG:      SETRESRET(te->hdeg);
        case GENLIST:
            /* TODO: recreate genlist if addgen had been invoked */
            SETRESRET(te->genlist);
    }

    Tcl_SetResult(ip,"internal error in Tcl_EnumCgetCmd",TCL_STATIC);
    return TCL_ERROR;
}

int Tcl_EnumBasisCmd(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd;

    if (TCL_OK != Tcl_EnumSetValues(cd, ip)) return TCL_ERROR;

    /* naive implementation: iterate through enum and create list */

    if (firstRedmon(te->enm))
        do {
            Tcl_Obj *aux =  Tcl_NewExmoCopyObj(&(te->enm->theex));
            Tcl_AppendElement(ip, Tcl_GetString(aux));
            DECREFCNT(aux);
        } while (nextRedmon(te->enm));

    return TCL_OK;
}

int Tcl_EnumDimensionCmd(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd;
    int res;

    if (objc != 2) RETERR("too many arguments");

    if (TCL_OK != Tcl_EnumSetValues(cd, ip)) return TCL_ERROR;

    res = DimensionFromEnum(te->enm);

    Tcl_SetObjResult(ip, Tcl_NewIntObj(res));

    return TCL_OK;
}

int Tcl_EnumSeqnoCmd(ClientData cd, Tcl_Interp *ip, int usemotivic,
		     int objc, Tcl_Obj * const objv[]) {
  tclEnum *te = (tclEnum *) cd;
  enumerator *enu;
  int res;

  if (objc != 3) {
    Tcl_WrongNumArgs(ip, 2, objv, "(<monomial> or <enumerator>)");
    return TCL_ERROR;
  }

  if (TCL_OK != Tcl_EnumSetValues(cd, ip)) return TCL_ERROR;

  if (!Tcl_ObjIsExmo(objv[2]))
    if (NULL != (enu = Tcl_EnumFromObj(ip, objv[2]))) {

      int max = DimensionFromEnum(te->enm);
      Tcl_Obj *res = Tcl_NewObj();

      if (firstRedmon(enu))
	do {
	  int sqn;
	  exmo aux, *eptr;

	  if(usemotivic) {
	    copyExmo(&aux,&(enu->theex));
	    motateExmo(&aux);
	    eptr = &aux;
	  } else {
	    eptr = &(enu->theex);
	  }
	  sqn = SeqnoFromEnum(te->enm, eptr);

	  if(sqn<0) {
	    Tcl_SetResult(ip, "cannot account for monomial ", TCL_STATIC);
	    Tcl_Obj *e = Tcl_NewExmoCopyObj(eptr);
	    Tcl_IncrRefCount(e);
	    Tcl_AppendResult(ip, Tcl_GetString(e), NULL);
	    Tcl_DecrRefCount(e);
	    Tcl_DecrRefCount(res);
	    return TCL_ERROR;
	  }

	  if ((sqn < 0) || (sqn >= max)) sqn = -1;

	  if (TCL_ERROR == Tcl_ListObjAppendElement(ip, res, Tcl_NewIntObj(sqn))) {
	    DECREFCNT(res);
	    return TCL_ERROR;
	  }
	} while (nextRedmon(enu));

      Tcl_SetObjResult(ip, res);
      return TCL_OK;
    }

  if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
    return TCL_ERROR;

  if(usemotivic) {
    exmo aux;
    copyExmo(&aux,exmoFromTclObj(objv[2]));
    motateExmo(&aux);
    res = SeqnoFromEnum(te->enm, &aux);
  } else {
    res = SeqnoFromEnum(te->enm, exmoFromTclObj(objv[2]));
  }
  Tcl_SetObjResult(ip, Tcl_NewIntObj(res));
  return TCL_OK;
}

int Tcl_EnumSiglistCmd(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd;
    exmo sig; int sideg, sedeg;
    void *poly;

    if (objc != 2) RETERR("wrong number of arguments");

    if (TCL_OK != Tcl_EnumSetValues(cd, ip)) return TCL_ERROR;

    memset(&sig, 0, sizeof(exmo));
    sideg = sedeg = 0;
    if (NULL == (poly = PLcreate(stdpoly)))
        RETERR("out of memory");
    do {
        if (SUCCESS != PLappendExmo(stdpoly, poly, &sig)) {
            PLfree(stdpoly, poly);
            RETERR("out of memory");
        }
    } while (nextSignature(te->enm, &sig, &sideg, &sedeg));

    Tcl_SetObjResult(ip, Tcl_NewPolyObj(stdpoly, poly));
    return TCL_OK;
}

int Tcl_EnumDecodeCmd(ClientData cd, Tcl_Interp *ip, Tcl_Obj *obj, int scale) {
    tclEnum *te = (tclEnum *) cd;
    vectorType *vt; void *vdat;
    void *pdat;
    int vdim, edim, idx, val, prime;

    if (TCL_OK != Tcl_EnumSetValues(cd, ip)) return TCL_ERROR;

    if (!Tcl_ObjIsVector(obj))
        ASSERT(NULL == "Tcl_EnumDecodeCmd expects vector argument");

    prime = te->enm->pi->prime;
    scale %= prime;

    vt   = vectorTypeFromTclObj(obj);
    vdat = vectorFromTclObj(obj);
    vdim = vt->getLength(vdat);

    edim = DimensionFromEnum(te->enm);

    if (vdim != edim) {
        char err[100];
        sprintf(err,"dimension mismatch: %d (vector) != %d (enumerator)",
                vdim, edim);
        RETERR(err);
    }

    if (NULL == (pdat = PLcreate(stdpoly))) RETERR("PLcreate failed");

    idx = 0;
    if (firstRedmon(te->enm))
        do {
            if (SUCCESS != (vt->getEntry(vdat, idx, &val))) {
                PLfree(stdpoly, pdat);
                if (idx > edim) {
                    char err[200];
                    sprintf(err, "internal error in Tcl_EnumDecodeCmd (%d > %d)",
                            idx, edim);
                    RETERR(err);
                }
                RETERR("internal error in Tcl_EnumDecodeCmd: vt->getEntry failed");
            }

            val *= scale;
            val %= prime;
            if (val < 0) val += prime;

            if (val) {
                te->enm->theex.coeff = val;
                if (SUCCESS != PLappendExmo(stdpoly, pdat, &(te->enm->theex))) {
                    PLfree(stdpoly, pdat);
                    RETERR("internal error in Tcl_EnumDecodeCmd: "
                           "PLappendExmo failed");
                }
            }

            idx++;
        } while (nextRedmon(te->enm));

    Tcl_SetObjResult(ip, Tcl_NewPolyObj(stdpoly, pdat));

    return TCL_OK;
}

int Tcl_EnumEncodeCmd(ClientData cd, Tcl_Interp *ip, Tcl_Obj *obj) {
    tclEnum *te = (tclEnum *) cd;
    vectorType *vt; void *vdat;
    polyType   *pt; void *pdat;
    int edim, pdim, idx, sqn, prime;
    exmo ex;

    if (TCL_OK != Tcl_EnumSetValues(cd, ip)) return TCL_ERROR;

    prime = te->enm->pi->prime;

    if (!Tcl_ObjIsPoly(obj))
        ASSERT(NULL == "Tcl_EnumEncodeCmd expects polynomial argument");

    pt   = polyTypeFromTclObj(obj);
    pdat = polyFromTclObj(obj);

    edim = DimensionFromEnum(te->enm);
    pdim = PLgetNumsum(pt, pdat);

    vt = stdvector;
    if (NULL == (vdat = vt->createVector(edim)))
        RETERR("out of memory");

    for (idx=0; idx<pdim; idx++) {
        if (SUCCESS != PLgetExmo(pt, pdat, &ex, idx)) {
            vt->destroyVector(vdat);
            RETERR("internal error in Tcl_EnumEncodeCmd: PLgetExmo failed");
        }

        sqn = SeqnoFromEnum(te->enm, &ex);

        if ((sqn<0) || (sqn>=edim)) {
            Tcl_Obj *aux;
            char err[200];
            vt->destroyVector(vdat);
            aux = Tcl_NewExmoCopyObj(&ex);
            sprintf(err,"could not find {%s} in basis (found seqno = %d)",
                    Tcl_GetString(aux), sqn);
            DECREFCNT(aux);
            RETERR(err);
        }

        if (SUCCESS != vt->setEntry(vdat, sqn, ex.coeff % prime)) {
            vt->destroyVector(vdat);
            RETERR("internal error in Tcl_EnumEncodeCmd: vt->setEntry failed");
        }
    }

    Tcl_SetObjResult(ip, Tcl_NewVectorObj(vt, vdat));

    return TCL_OK;
}

#if USEOPENCL
typedef struct {
    stcl_context *ctx;
    tclEnum *te;
    enumerator *enm;
    int more;
    int algdim;
    Tcl_Obj *lengthvar;
    Tcl_Obj *genvar;
    Tcl_Obj *buf;
    Tcl_Obj *waitvar;
    Tcl_Obj *bdy;
} clenumbasiscbdata;

static void eventcbfreepoly (cl_event event, cl_int event_command_exec_status, void *user_data) {
    stp *p = (stp *) user_data;
    stdpoly->free(p);
}

int STcl_EnumBasisPostProc(ClientData data[], Tcl_Interp *ip, int result) {
    clenumbasiscbdata *cb = (clenumbasiscbdata *)data[0];
    //fprintf(stderr,"res=%d, cb->more=%d, cb->algdim=%d\n",result,cb->more,cb->algdim);
    if(cb->more && (TCL_OK == result || TCL_CONTINUE == result)) {
        stp *p = (stp *)stdpoly->createCopy(NULL);
        stdRealloc(p,cb->algdim);
        int gen = cb->te->enm->theex.gen;
        int len = cb->algdim;
        while(cb->enm->theex.gen == gen && cb->more) {
            stdpoly->appendExmo(p,&(cb->enm->theex));
            cb->more = nextRedmonWithAlgDim(cb->enm,&(cb->algdim));
        }

        result = TCL_OK;

        Tcl_ObjSetVar2(ip,cb->genvar,NULL,Tcl_NewIntObj(gen),0);
        Tcl_ObjSetVar2(ip,cb->lengthvar,NULL,Tcl_NewIntObj(len),0);
        size_t bufsz = sizeof(exmo)*len;
        cl_int rc;
        cl_event evt;
        cl_mem buf = clCreateBuffer(cb->ctx->ctx, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
                                    bufsz, NULL, &rc);
        if(CL_SUCCESS == rc && buf) {
            cl_command_queue q = GetOrCreateCommandQueue(ip,cb->ctx,0);
            rc = clEnqueueWriteBuffer(q,buf,0/*blocking*/,0/*offset*/,bufsz,&(p->dat[0]),0,NULL,&evt);
        }
        if(CL_SUCCESS != rc) {
            stdpoly->free(p);
            SetCLErrorCode(ip,rc);
            result = TCL_ERROR;
        } else {
            clSetEventCallback(evt, CL_COMPLETE, eventcbfreepoly, p);
            if (TCL_OK != STcl_SetEventTrace(ip, Tcl_GetString(cb->waitvar), evt)) {
                SetCLErrorCode(ip,rc);
                result = TCL_ERROR;
            } else if(TCL_OK != STcl_CreateMemObj(ip, cb->buf, buf)) {
                SetCLErrorCode(ip,rc);
                result = TCL_ERROR;
            } else {
                Tcl_NRAddCallback(ip, STcl_EnumBasisPostProc, cb, 0, 0, 0);
                return Tcl_NREvalObj(ip, cb->bdy, 0);
            }
        }
    }
    free(cb);
    return result;
}

int STcl_EnumBasis(Tcl_Interp *ip, tclEnum *te, int objc, Tcl_Obj * const objv[]) {
    stcl_context *ctx;
    if(TCL_OK != STcl_GetContext(ip, &ctx)) return TCL_ERROR;
    clenumbasiscbdata *cb = (clenumbasiscbdata *)malloc(sizeof(clenumbasiscbdata));
    if(NULL == cb) {
        Tcl_SetResult(ip,"out of memory",TCL_STATIC);
        return TCL_ERROR;
    }
    cb->ctx = ctx;
    cb->lengthvar = objv[2];
    cb->genvar = objv[3];
    cb->buf = objv[4];
    cb->waitvar = objv[5];
    cb->bdy = objv[6];
    cb->te = te;
    cb->enm = te->enm;
    cb->more = firstRedmonWithAlgDim(cb->enm,&(cb->algdim));
    Tcl_NRAddCallback(ip, STcl_EnumBasisPostProc, cb, 0, 0, 0);
    return TCL_OK;
}

int STcl_EnumMap(Tcl_Interp *ip, tclEnum *te, int objc, Tcl_Obj * const objv[]) {
    stcl_context *ctx;
    if(TCL_OK != STcl_GetContext(ip, &ctx)) return TCL_ERROR;
    int totdim = DimensionFromEnum(te->enm);
    if(totdim<0) {
        Tcl_SetResult(ip, "internal error in DimensionFromEnum", TCL_STATIC);
        return TCL_ERROR;
    }
    // DimensionFromEnum has initialised or updated the seqoff and seqtab tables
    int tl = te->cl.tablen = te->enm->tablen, *st, *st2;
    st = (int*) malloc(sizeof(int)*NALG*te->enm->tablen);
    if(NULL == st) {
        Tcl_SetResult(ip, "out of memory", TCL_STATIC);
        return TCL_ERROR;
    }
    st2 = st;
    for(int k=0;k<NALG;k++)
        for(int j=0;j<tl;j++)
            *st2++ = te->enm->seqtab[k][j];
    if(te->cl.seqtab) free(te->cl.seqtab);
    te->cl.seqtab = st;
    int gcnt = te->enm->efflen, maxid=-1;
    for(int k=0;k<gcnt;k++) {
        effgen *eg = &(te->enm->efflist[k]);
        if(eg->ext) {
            Tcl_SetResult(ip, "exterior algebra not supported", TCL_STATIC);
        }
        if(maxid < eg->id) maxid = eg->id;
    }
    if(maxid<0) {
        maxid = 1;
    }
    st = (int*)malloc(maxid * sizeof(int));
    if(NULL == st) {
        Tcl_SetResult(ip, "out of memory", TCL_STATIC);
        return TCL_ERROR;
    }
    for(int k=0;k<gcnt;k++) {
        effgen *eg = &(te->enm->efflist[k]);
        st[eg->id] = te->enm->seqoff[k];
    }
    if(te->cl.offsets) free(te->cl.offsets);
    te->cl.gencnt = gcnt;
    te->cl.offsets = st;

    for(int k=0;k<NALG;k++) {
        int prd, prd2, maxdeg;
        int rdgk = (2<<k)-1;
        prd = te->enm->profile.r.dat[k];
        prd2 = te->enm->algebra.r.dat[k];
        if(prd2<prd) prd = prd2;
        maxdeg = (te->enm->algebra.r.dat[k] - prd) * rdgk;
        te->cl.maxdeg[k] = maxdeg;
        te->cl.profile[k] = te->enm->profile.r.dat[k];
    }

#define NUMENMBUF 3
    void *buffer[NUMENMBUF];
    int   buflen[NUMENMBUF];
    Tcl_Obj *bufvar[NUMENMBUF];

    buffer[0] = &(te->cl);
    buflen[0] = sizeof(clenum);
    bufvar[0] = objv[2];

    buffer[1] = te->cl.seqtab;
    buflen[1] = NALG*te->cl.tablen*sizeof(int);
    bufvar[1] = objv[3];

    buffer[2] = te->cl.offsets;
    buflen[2] = te->cl.gencnt*sizeof(int);
    bufvar[2] = objv[4];

    int wvpos = 5;
    for(int k=0;k<NUMENMBUF;k++,wvpos++) {
        cl_int rc;
        int blocking;
        cl_event evt, *evtptr;
        if(wvpos<objc) {
            blocking = 0;
            evtptr = &evt;
        } else {
            blocking = 1;
            evtptr = NULL;
        }
        cl_mem buf = clCreateBuffer(ctx->ctx, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
                                    buflen[k], NULL, &rc);
        if(CL_SUCCESS == rc && buf) {
            cl_command_queue q = GetOrCreateCommandQueue(ip,ctx,0);
            rc = clEnqueueWriteBuffer(q,buf, blocking,0/*offset*/,buflen[k],buffer[k],0,NULL,evtptr);
        }
        if(CL_SUCCESS != rc) {
            SetCLErrorCode(ip,rc);
            return TCL_ERROR;
        }
        if(evtptr) {
            if (TCL_OK != STcl_SetEventTrace(ip, Tcl_GetString(objv[wvpos]), evt)) {
                SetCLErrorCode(ip,rc);
                return TCL_ERROR;
            }
        }
        if(TCL_OK != STcl_CreateMemObj(ip, bufvar[k], buf)) {
            SetCLErrorCode(ip,rc);
            return TCL_ERROR;
        }
    }

    return TCL_OK;
}
#endif

typedef enum { CGET, CONFIGURE, BASIS, SEQNO, SEQNOMOT, DIMENSION, TEST,
               SIGRESET, SIGNEXT, SIGLIST, DECODE, ENCODE, ENM_MAX, ENM_MIN,
               CLMAP, CLBASIS } enumcmdcode;

static const char *cmdNames[] = { "test", "cget", "configure", "min", "max",
                                  "basis", "seqno", "motseqno", "dimension",
                                  "sigreset", "signext", "siglist",
                                  "decode", "encode",
                                  "clmap", "clbasis",
                                  (char *) NULL };

static enumcmdcode cmdmap[] = { TEST, CGET, CONFIGURE, ENM_MIN, ENM_MAX,
                                BASIS, SEQNO, SEQNOMOT, DIMENSION,
                                SIGRESET, SIGNEXT, SIGLIST,
                                DECODE, ENCODE,
                                CLMAP, CLBASIS };

static const char *degNames[] = { "idegree", "edegree", "hdegree", "generator", (char *) NULL };

int Tcl_EnumWidgetCmd(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * const objv[]) {

    tclEnum *te = (tclEnum *) cd;
    exmo ex;
    int result, index, index2, scale;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(ip, objv[1], cmdNames, "subcommand", 0, &index);
    if (result != TCL_OK) return result;

    switch (cmdmap[index]) {
        case TEST:
            if (objc != 2) {
                Tcl_WrongNumArgs(ip, 2, objv, NULL);
                return TCL_ERROR;
            }
            Tcl_EnumSetValues(cd, ip);
            return TCL_OK;

        case CGET:
            return Tcl_EnumCgetCmd(cd, ip, objc, objv);

        case CONFIGURE:
            return Tcl_EnumConfigureCmd(cd, ip, objc-2, objv+2);

        case BASIS:
            return Tcl_EnumBasisCmd(cd, ip, objc, objv);

        case SEQNO:
	        return Tcl_EnumSeqnoCmd(cd, ip, 0, objc, objv);

        case SEQNOMOT:
	        return Tcl_EnumSeqnoCmd(cd, ip, 1, objc, objv);

        case DIMENSION:
            return Tcl_EnumDimensionCmd(cd, ip, objc, objv);

        case ENM_MIN:
        case ENM_MAX:
            if (objc != 3) {
                Tcl_WrongNumArgs(ip, 2, objv, "parameterName");
                return TCL_ERROR;
            }

            result = Tcl_GetIndexFromObj(ip, objv[2], degNames, "parameter", 0, &index2);
            if (result != TCL_OK) return result;

            if (TCL_OK != Tcl_EnumSetValues(cd, ip)) return TCL_ERROR;

            if (ENM_MIN == cmdmap[index])
                switch (index2) {
                    case 0: result = te->enm->minideg; break;
                    case 1: result = te->enm->minedeg; break;
                    case 2: result = te->enm->minhdeg; break;
                    case 3: result = te->enm->mingen; break;
                }
            else
                switch (index2) {
                    case 0: result = te->enm->maxideg; break;
                    case 1: result = te->enm->maxedeg; break;
                    case 2: result = te->enm->maxhdeg; break;
                    case 3: result = te->enm->maxgen; break;
                }

            Tcl_SetObjResult(ip, Tcl_NewIntObj(result));
            return TCL_OK;

        case SIGRESET:
            if (objc != 2) RETERR("wrong number of arguments");
            TRYFREEOBJ(te->sig);
            memset(&ex, 0, sizeof(exmo));
            te->sig = Tcl_NewExmoCopyObj(&ex); INCREFCNT(te->sig); te->csig = 1;
            return TCL_OK;

        case SIGNEXT:
            if (objc != 2) RETERR("wrong number of arguments");
            if (TCL_OK != Tcl_EnumSetValues(cd, ip)) return TCL_ERROR;
            result = enmIncrementSig(te->enm);
            TRYFREEOBJ(te->sig);
            te->sig = needsUpdate;
            Tcl_SetObjResult(ip, Tcl_NewIntObj(result));
            return TCL_OK;

        case SIGLIST:
            return Tcl_EnumSiglistCmd(cd, ip, objc, objv);

        case DECODE:
            if ((objc < 3) || (objc > 4)) {
                Tcl_WrongNumArgs(ip, 2, objv, "vector ?scale?");
                return TCL_ERROR;
            }

            if (TCL_OK != Tcl_ConvertToVector(ip, objv[2]))
                return TCL_ERROR;

            scale = 1;

            if (objc==4)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &scale))
                    return TCL_ERROR;

            return Tcl_EnumDecodeCmd(cd, ip, objv[2], scale);

        case ENCODE:

            if (objc!=3) {
                Tcl_WrongNumArgs(ip, 2, objv, "polynomial");
                return TCL_ERROR;
            }

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            return Tcl_EnumEncodeCmd(cd, ip, objv[2]);

        case CLMAP:
        {
#if USEOPENCL
            if (objc<5) {
                Tcl_WrongNumArgs(ip, 2, objv, "enumbuf seqtabbuf offsetbuf ?waitvar1? ...");
                return TCL_ERROR;
            }
            if (TCL_OK != Tcl_EnumSetValues(te, ip)) return TCL_ERROR;
            return STcl_EnumMap(ip,te,objc,objv);
#else
            Tcl_SetResult(ip, "not compiled with OpenCL support", TCL_STATIC);
            return TCL_ERROR;
#endif
        }
        case CLBASIS:
        {
#if USEOPENCL
            if (objc!= 7) {
                Tcl_WrongNumArgs(ip, 2, objv, "lenvar genvar buffer waitvar bdy");
                return TCL_ERROR;
            }
            if (TCL_OK != Tcl_EnumSetValues(te, ip)) return TCL_ERROR;
            return STcl_EnumBasis(ip,te,objc,objv);
#else
            Tcl_SetResult(ip, "not compiled with OpenCL support", TCL_STATIC);
            return TCL_ERROR;
#endif
        }
    }

    Tcl_SetResult(ip, "internal error in Tcl_EnumWidgetCmd", TCL_STATIC);
    return TCL_ERROR;
}

void Tcl_DestroyEnum(ClientData cd) {
    tclEnum *te = (tclEnum *) cd;

    TRYFREEOBJ(te->prime);
    TRYFREEOBJ(te->alg);
    TRYFREEOBJ(te->pro);
    TRYFREEOBJ(te->sig);
    TRYFREEOBJ(te->ideg);
    TRYFREEOBJ(te->edeg);
    TRYFREEOBJ(te->hdeg);
    TRYFREEOBJ(te->genlist);
    TRYFREEOBJ(te->ispos);
    if (NULL != te->gl) freex(te->gl);
    if (NULL != te->enm) {
        enmDestroy(te->enm);
        freex(te->enm);
    }
#if USEOPENCL
    if(te->cl.seqtab) free(te->cl.seqtab);
    if(te->cl.offsets) free(te->cl.offsets);
#endif
    freex(te);
}

enumerator *Tcl_EnumFromObj(Tcl_Interp *ip, Tcl_Obj *obj) {
    Tcl_CmdInfo info;
    tclEnum *te;

    if (!Tcl_GetCommandInfo(ip, Tcl_GetString(obj), &info)) {
        Tcl_SetResult(ip, "command not found", TCL_STATIC);
        return NULL;
    }

    if (info.objProc != Tcl_EnumWidgetCmd) {
        Tcl_SetResult(ip, "enumerator object expected", TCL_STATIC);
        return NULL;
    }

    te = (tclEnum *) info.objClientData;

    if (TCL_OK != Tcl_EnumSetValues((ClientData) te, ip))
        return NULL;

    return te->enm;
}

int Tcl_CreateEnumCmd(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * const objv[]) {
    tclEnum *te;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv,  "name ?options?");
        return TCL_ERROR;
    }

    if (NULL == (te = (tclEnum*) callox(1, sizeof(tclEnum))))
        RETERR("out of memory");

    if (NULL == (te->enm = enmCreate()))
        RETERR("out of memory");

    /* set empty (= default) options */
    te->prime = te->alg = te->pro = te->sig = te->ideg = te->edeg = te->hdeg
        = te->genlist = NULL;

#if USEOPENCL
    te->cl.seqtab = NULL;
    te->cl.offsets = NULL;
#endif

    te->ispos = ourPosObj;
    INCREFCNT(te->ispos);

    /* mark all options as changed */
    te->cprime = te->calg = te->cpro = te->csig = te->cideg = te->cedeg = te->chdeg
        = te->cgenlist = 1;

    if (objc > 2)
        if (TCL_OK != Tcl_EnumConfigureCmd((ClientData) te, ip, objc-2, objv+2)) {
            Tcl_DestroyEnum((ClientData) te);
            return TCL_ERROR;
        }

    Tcl_CreateObjCommand(ip, Tcl_GetString(objv[1]),
                         Tcl_EnumWidgetCmd, (ClientData) te, Tcl_DestroyEnum);

    return TCL_OK;
}

int Tenum_Init(Tcl_Interp *ip) {

    Tcl_InitStubs(ip, "8.0", 0);

    Tptr_Init(ip);
    Tprime_Init(ip);
    Tpoly_Init(ip);

    if (NULL == ourPosObj)
        ourPosObj = Tcl_NewStringObj("positive", STRLEN("positive"));

    if (NULL == ourNegObj)
        ourNegObj = Tcl_NewStringObj("negative", STRLEN("negative"));

    INCREFCNT(ourPosObj);
    INCREFCNT(ourNegObj);

    Tcl_CreateObjCommand(ip, POLYNSP "enumerator",
                         Tcl_CreateEnumCmd, (ClientData) 0, NULL);

    return TCL_OK;
}
