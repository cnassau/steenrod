/*
 * Tcl interface to the enumerator structure
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

#include <tclInt.h>   /* for Tcl_GetCommand */
#include <string.h>
#include "tprime.h"
#include "tpoly.h"
#include "tlin.h"
#include "linwrp.h"
#include "poly.h"
#include "tenum.h"
#include "enum.h"

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
{ if (NULL != ip) Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; }

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
} tclEnum;

#define TRYFREEOBJ(obj) \
{ if ((NULL != (obj)) && (needsUpdate != (obj) && (defaultParameter != (obj)))) \
  { Tcl_DecrRefCount(obj); obj = NULL; }; }

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

static CONST char *typeNames[] = { "positive", "negative" };

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
        enmSetGenlist(te->enm, te->gl, te->gllength);
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

static CONST char *optNames[] = { "-prime", "-algebra", "-profile", "-signature",
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
        Tcl_Obj *(co[2]), *res = Tcl_NewListObj(0, NULL);

        /* describe current configuration status */

        if (needsUpdate == te->sig) {
            te->sig = Tcl_NewExmoCopyObj(&(te->enm->signature));
            Tcl_IncrRefCount(te->sig);
        } 

        if (needsUpdate == te->genlist) {
            /* TODO: recreate genlist from enum if that has been changed 
             * by a future addgen cmd */
        }

#define APPENDOPT(name, obj) {                                          \
co[0] = Tcl_NewStringObj(name,STRLEN(name));                            \
co[1] = (NULL != (obj)) ? (obj) : Tcl_NewObj();                         \
if (TCL_OK != Tcl_ListObjAppendElement(ip, res, Tcl_NewListObj(2,co)))  \
   { Tcl_DecrRefCount(res); return TCL_ERROR; };                        \
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
({ value = objv[1]; Tcl_GetStringFromObj(value, &length); \
 if (0 == length) var = value = defaultParameter; (0 == length); })

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
                Tcl_IncrRefCount(te->prime);
                te->cprime = 1;
            }
        }
    }

#define SETOPT(old,new,flag)                                           \
if (NULL != (new)) {                                                   \
   if (defaultParameter != (new)) {                                    \
      TRYFREEOBJ(old); (old) = (new); Tcl_IncrRefCount(old); flag = 1; \
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
        Tcl_IncrRefCount(te->genlist);
    }

    DOFLOG("Leaving Tcl_EnumConfigureCmd");

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
            Tcl_DecrRefCount(aux);
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

int Tcl_EnumSeqnoCmd(ClientData cd, Tcl_Interp *ip, 
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
                    int sqn = SeqnoFromEnum(te->enm, &(enu->theex));
                    
                    if ((sqn < 0) || (sqn >= max)) sqn = -1;

                    if (TCL_ERROR == Tcl_ListObjAppendElement(ip, res, 
                                                              Tcl_NewIntObj(sqn))) {
                        Tcl_DecrRefCount(res);
                        return TCL_ERROR;
                    }
                } while (nextRedmon(enu));
            
            Tcl_SetObjResult(ip, res);
            return TCL_OK;
        }

    if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
        return TCL_ERROR;
    
    res = SeqnoFromEnum(te->enm, exmoFromTclObj(objv[2]));
    
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

int Tcl_EnumDecodeCmd(ClientData cd, Tcl_Interp *ip, Tcl_Obj *obj) {
    tclEnum *te = (tclEnum *) cd;   
    vectorType *vt; void *vdat;
    void *pdat;
    int vdim, edim, idx, val;

    if (TCL_OK != Tcl_EnumSetValues(cd, ip)) return TCL_ERROR;

    if (!Tcl_ObjIsVector(obj))
        assert(NULL == "Tcl_EnumDecodeCmd expects vector argument");

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
        assert(NULL == "Tcl_EnumEncodeCmd expects polynomial argument");

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
            Tcl_DecrRefCount(aux);
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

typedef enum { CGET, CONFIGURE, BASIS, SEQNO, DIMENSION,
               SIGRESET, SIGNEXT, SIGLIST, DECODE, ENCODE } enumcmdcode;

static CONST char *cmdNames[] = { "cget", "configure", 
                                  "basis", "seqno", "dimension", 
                                  "sigreset", "signext", "siglist",
                                  "decode", "encode",
                                  (char *) NULL };

static enumcmdcode cmdmap[] = { CGET, CONFIGURE, BASIS, SEQNO, DIMENSION,
                                SIGRESET, SIGNEXT, SIGLIST, 
                                DECODE, ENCODE };

int Tcl_EnumWidgetCmd(ClientData cd, Tcl_Interp *ip, 
                      int objc, Tcl_Obj * const objv[]) {

    tclEnum *te = (tclEnum *) cd;
    Tcl_Obj *auxobj;
    exmo ex;
    int result, index;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(ip, objv[1], cmdNames, "subcommand", 0, &index);
    if (result != TCL_OK) return result;
    
    switch (cmdmap[index]) {
        case CGET: 
            return Tcl_EnumCgetCmd(cd, ip, objc, objv);
        case CONFIGURE:
            return Tcl_EnumConfigureCmd(cd, ip, objc-2, objv+2);
        case BASIS:
            return Tcl_EnumBasisCmd(cd, ip, objc, objv);
        case SEQNO:
            return Tcl_EnumSeqnoCmd(cd, ip, objc, objv);
        case DIMENSION:
            return Tcl_EnumDimensionCmd(cd, ip, objc, objv);            
        case SIGRESET:
            if (objc != 2) RETERR("wrong number of arguments");
            memset(&ex, 0, sizeof(exmo));
            auxobj = Tcl_NewExmoCopyObj(&ex);
            SETOPT(te->sig, auxobj, te->csig);
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
            if (objc != 3) {
                Tcl_WrongNumArgs(ip, 2, objv, "<vector>");
                return TCL_ERROR;
            }
            if (TCL_OK != Tcl_ConvertToVector(ip, objv[2])) 
                return TCL_ERROR;
            return Tcl_EnumDecodeCmd(cd, ip, objv[2]);
        case ENCODE:
            if (objc!=3) {
                Tcl_WrongNumArgs(ip, 2, objv, "<polynomial>");
                return TCL_ERROR;
            }
            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2])) 
                return TCL_ERROR;
            return Tcl_EnumEncodeCmd(cd, ip, objv[2]);
    }

    Tcl_SetResult(ip, "internal error in Tcl_EnumWidgetCmd", TCL_STATIC);
    return TCL_ERROR;
} 

void Tcl_DestroyEnum(ClientData cd) {
    tclEnum *te = (tclEnum *) cd;

    TRYFREEOBJ(te->prime);
    TRYFREEOBJ(te->alg);
    TRYFREEOBJ(te->pro);
    TRYFREEOBJ(te->ideg);
    TRYFREEOBJ(te->edeg);
    TRYFREEOBJ(te->hdeg);
    TRYFREEOBJ(te->genlist);
    if (NULL != te->enm) enmDestroy(te->enm);
    freex(te);
}

enumerator *Tcl_EnumFromObj(Tcl_Interp *ip, Tcl_Obj *obj) {
    Tcl_Command cmd;
    Tcl_CmdInfo info;
    tclEnum *te;
    
    cmd = Tcl_GetCommandFromObj(ip, obj);

    if (NULL == cmd) Tcl_SetResult(ip, "command not found", TCL_STATIC);

    if (!Tcl_GetCommandInfoFromToken(cmd, &info))
        return NULL;

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
                      int objc, Tcl_Obj * CONST objv[]) {
    tclEnum *te;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv,  "name ?options?");
        return TCL_ERROR;
    }
    
    if (NULL == (te = callox(1, sizeof(tclEnum))))
        RETERR("out of memory");

    if (NULL == (te->enm = enmCreate())) 
        RETERR("out of memory");

    /* set empty (= default) options */
    te->prime = te->alg = te->pro = te->sig = te->ideg = te->edeg = te->hdeg 
        = te->genlist = NULL;

    te->ispos = ourPosObj; Tcl_IncrRefCount(ourPosObj);

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

    Tcl_IncrRefCount(ourPosObj); 
    Tcl_IncrRefCount(ourNegObj); 

    Tcl_CreateObjCommand(ip, POLYNSP "enumerator", 
                         Tcl_CreateEnumCmd, (ClientData) 0, NULL);

    return TCL_OK;
}
