/*
 * Tcl interface to the polynomial routines
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

#define DBGPOLY if (0)

#include "tptr.h"
#include "tpoly.h"
#include "tprime.h"
#include "poly.h"
#include "mult.h"

#include <string.h>

#define FREETCLOBJ(obj) { INCREFCNT(obj); DECREFCNT(obj); }

#define RETERR(errmsg) \
{ if (NULL != ip) Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; } 

#define RETINT(i) { Tcl_SetObjResult(ip, Tcl_NewIntObj(i)); return TCL_OK; }

#define GETINT(ob, var)                                \
 if (TCL_OK != Tcl_GetIntFromObj(ip, ob, &privateInt)) \
    return TCL_ERROR;                                  \
 var = privateInt;

#define PTR1(objptr) ((objptr)->internalRep.twoPtrValue.ptr1)
#define PTR2(objptr) ((objptr)->internalRep.twoPtrValue.ptr2)

/**************************************************************************
 *
 * The Tcl type for extended monomials. We use PTR1 as a pointer to 
 * an exmo structure. The string representation takes the form 
 *  
 *  { coefficient exterior {list of exponents} generator }        
 */

static int monCount;

#if 0
#  define INCMONCNT \
  { fprintf(stderr, "monCount = %d (%s, %d)\n", ++monCount, __FILE__, __LINE__); }
#  define DECMONCNT \
  { fprintf(stderr, "monCount = %d (%s, %d)\n", --monCount, __FILE__, __LINE__); }
#else
#  define INCMONCNT { ++monCount; }
#  define DECMONCNT { --monCount; }
#endif

static Tcl_ObjType tclExmo;

int Tcl_ConvertToExmo(Tcl_Interp *ip, Tcl_Obj *obj) {
    return Tcl_ConvertToType(ip, obj, &tclExmo);
}

int Tcl_ObjIsExmo(Tcl_Obj *obj) { return &tclExmo == obj->typePtr; }

exmo *exmoFromTclObj(Tcl_Obj *obj) {
    if (&tclExmo == obj->typePtr) return (exmo *) PTR1(obj);
    return NULL;
}

Tcl_Obj *Tcl_NewExmoObj(exmo *ex) {
    Tcl_Obj *res = Tcl_NewObj();
    PTR1(res) = ex; 
    res->typePtr = &tclExmo;
    Tcl_InvalidateStringRep(res);
    INCMONCNT;
    return res;
}

Tcl_Obj *Tcl_NewExmoCopyObj(exmo *ex) {
    exmo *x = mallox(sizeof(exmo));
    TCLMEMASSERT(x);
    copyExmo(x,ex);
    return Tcl_NewExmoObj(x);
}

/* free internal representation */
void ExmoFreeInternalRepProc(Tcl_Obj *obj) {
    freex(PTR1(obj));
    DECMONCNT;
}

#define FREEEANDRETERR { freex((char *) e); return TCL_ERROR; }

/* try to turn objPtr into an Exmo */
int ExmoSetFromAnyProc(Tcl_Interp *ip, Tcl_Obj *objPtr) {
    int objc, objc2, i, pad=0, aux;
    Tcl_Obj **objv, **objv2;
    exmo *e;

#ifdef TCL_MEM_DEBUG
    if (0) Tcl_DumpActiveMemory("actmem.dbg");
#endif

    if (TCL_OK != Tcl_ListObjGetElements(ip, objPtr, &objc, &objv))
        return TCL_ERROR;
    if (4 != objc) 
        RETERR("malformed monomial: wrong number of entries");
    if (NULL == (e = (exmo *) mallox(sizeof(exmo))))
        RETERR("out of memory");
    if (TCL_OK != Tcl_GetIntFromObj(ip,objv[0],&aux)) 
        FREEEANDRETERR;
    e->coeff = aux;
    if (TCL_OK != Tcl_GetIntFromObj(ip,objv[1],&aux)) 
        FREEEANDRETERR;
    e->ext = aux;
    if (TCL_OK != Tcl_GetIntFromObj(ip,objv[3],&(e->gen))) 
        FREEEANDRETERR;
    if (TCL_OK != Tcl_ListObjGetElements(ip, objv[2], &objc2, &objv2))
        return TCL_ERROR;
    if (objc2 > NALG) 
        RETERR("exponent sequence too long");
    
    aux = e->ext;
    for (i=0;i<objc2;i++) {
        if (TCL_OK != Tcl_GetIntFromObj(ip,objv2[i],&aux)) 
            FREEEANDRETERR;
        e->r.dat[i] = aux;
    }
    pad = (aux < 0) ? -1 : 0;
    for (;i<NALG;) e->r.dat[i++] = pad;

    TRYFREEOLDREP(objPtr);
    PTR1(objPtr) = e;
    objPtr->typePtr = &tclExmo;
    INCMONCNT;

    return TCL_OK;
}

/* Create a new list Obj from the objPtr */
Tcl_Obj *Tcl_NewListFromExmo(Tcl_Obj *objPtr) {
    exmo *e = (exmo *) PTR1(objPtr);
    Tcl_Obj *res, *(objv[4]), **arr; 
    int i, len = exmoGetRedLen(e);
    arr = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *)*len);
    for (i=0;i<len;i++) arr[i] = Tcl_NewIntObj(e->r.dat[i]);
    objv[0] = Tcl_NewIntObj(e->coeff);
    objv[1] = Tcl_NewIntObj(e->ext);
    objv[3] = Tcl_NewIntObj(e->gen);
    objv[2] = Tcl_NewListObj(len,arr);
    res = Tcl_NewListObj(4,objv);
    ckfree((char *) arr);
    return res;
}

/* recreate string representation */
void ExmoUpdateStringProc(Tcl_Obj *objPtr) {
    Tcl_Obj *aux = Tcl_NewListFromExmo(objPtr);
    copyStringRep(objPtr, aux);
    FREETCLOBJ(aux);
}

/* create copy */
void ExmoDupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    exmo *new = (exmo *) mallox(sizeof(exmo));
    TCLMEMASSERT(new); 
    memcpy(new, PTR1(srcPtr), sizeof(exmo));
    PTR1(dupPtr) = new;
    dupPtr->typePtr = srcPtr->typePtr;
    INCMONCNT;
}

/**************************************************************************
 *
 * The Tcl type for generic polynomials. To the Tcl user such a thing 
 * behaves like a list of extended monomials. 
 *
 * The implementation lets PTR1 point to the polyType and PTR2 to the data. 
 */

static int polCount;

#if 0
#  define INCPOLCNT \
  { fprintf(stderr, "polCount = %d (%s, %d)\n", ++polCount, __FILE__, __LINE__); }
#  define DECPOLCNT \
  { fprintf(stderr, "polCount = %d (%s, %d)\n", --polCount, __FILE__, __LINE__); }
#else
#  define INCPOLCNT { ++polCount; }
#  define DECPOLCNT { --polCount; }
#endif

Tcl_ObjType tclPoly;

int Tcl_ConvertToPoly(Tcl_Interp *ip, Tcl_Obj *obj) {
    return Tcl_ConvertToType(ip, obj, &tclPoly);
}

#define LOGPOLY(obj) \
DBGPOLY printf("  typePtr = %p, polyPtr = %p, isShared = %d\n", \
   PTR1(obj), PTR2(obj), Tcl_IsShared(obj)); 


int Tcl_ObjIsPoly(Tcl_Obj *obj) { return &tclPoly == obj->typePtr; }

polyType *polyTypeFromTclObj(Tcl_Obj *obj) { return (polyType *) PTR1(obj); }
void     *polyFromTclObj(Tcl_Obj *obj)     { return PTR2(obj); }

Tcl_Obj *Tcl_NewPolyObj(polyType *tp, void *data) {
    Tcl_Obj *res = Tcl_NewObj();
    PTR1(res) = (void *) tp;
    PTR2(res) = data;
    res->typePtr = &tclPoly;
    INCPOLCNT;
    Tcl_InvalidateStringRep(res);
    DBGPOLY printf("Tcl_NewPolyObj: created poly obj at %p, data at %p\n",res,data);
    return res;
}

Tcl_Obj *Tcl_NewListFromPoly(Tcl_Obj *obj) {
    int i, len = PLgetNumsum((polyType *) PTR1(obj),PTR2(obj));
    Tcl_Obj *res, **arr = (Tcl_Obj **) ckalloc(len * sizeof(Tcl_Obj *));
    exmo aux;
    for (i=0;i<len;i++) {
        PLgetExmo((polyType *) PTR1(obj),PTR2(obj),&aux,i);
        arr[i] = Tcl_NewExmoCopyObj(&aux);
    }
    res = Tcl_NewListObj(len,arr);
    ckfree((char *) arr);
    return res;
}

/* free internal representation */
void PolyFreeInternalRepProc(Tcl_Obj *obj) {
    DBGPOLY printf("PolyFreeInternalRepProc obj = %p\n",obj);
    LOGPOLY(obj);
    PLfree((polyType *) PTR1(obj),PTR2(obj));
    DECPOLCNT;
    DBGPOLY printf("Leaving PolyFreeInternalRepProc\n");
}

/* try to turn objPtr into a Poly */
int PolySetFromAnyProc(Tcl_Interp *ip, Tcl_Obj *objPtr) {
    int objc, i;
    void *pol;
    Tcl_Obj **objv;
    DBGPOLY printf("PolySetFromAnyProc obj = %p\n",objPtr);
    if (TCL_OK != Tcl_ListObjGetElements(ip, objPtr, &objc, &objv))
        return TCL_ERROR;
    if (NULL == (pol = (stdpoly->createCopy)(NULL))) 
        RETERR("out of memory");
    for (i=0;i<objc;i++) {
        if (TCL_OK != Tcl_ConvertToExmo(ip,objv[i])) { 
            (stdpoly->free)(pol); 
            return TCL_ERROR;
        }
        if (SUCCESS != PLappendExmo(stdpoly,pol,exmoFromTclObj(objv[i]))) {
            (stdpoly->free)(pol);
            RETERR("out of memory");
        }
    }

    TRYFREEOLDREP(objPtr);
    PTR1(objPtr) = stdpoly;
    PTR2(objPtr) = pol;
    objPtr->typePtr = &tclPoly;
    INCPOLCNT;

    LOGPOLY(objPtr);

    return TCL_OK;
}

/* recreate string representation */
void PolyUpdateStringProc(Tcl_Obj *objPtr) {
    Tcl_Obj *aux;
    DBGPOLY printf("PolyUpdateStringProc obj = %p\n",objPtr);
    LOGPOLY(objPtr);
    aux = Tcl_NewListFromPoly(objPtr);
    copyStringRep(objPtr, aux);
    FREETCLOBJ(aux);
}

/* create copy */
void PolyDupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    polyType *stp = (polyType *) PTR1(srcPtr);
    DBGPOLY printf("PolyDupInternalRepProc src = %p, dup = %p\n",srcPtr, dupPtr);
    LOGPOLY(srcPtr);
    PTR1(dupPtr) = (void *) stp;
    PTR2(dupPtr) = PLcreateCopy(stp,stp,PTR2(srcPtr));
    DBGPOLY printf("dupPtr poly now at %p\n", PTR2(dupPtr));
    dupPtr->typePtr = &tclPoly;
    INCPOLCNT;
}

/**** Tcl wrappers for some of the PL functions */

/* for some functions we need to convert the type first */
void Tcl_PolyObjConvert(Tcl_Obj *obj, polyType *newtype) {
    void *aux;
    DBGPOLY printf("Tcl_PolyObjConvert obj = %p\n", obj);
    if (PTR1(obj) == newtype) return;
    if (Tcl_IsShared(obj))
        ASSERT(NULL == "Tcl_PolyObjConvert called for shared object");
    aux = PLcreateCopy(newtype,PTR1(obj),PTR2(obj));
    PLfree(PTR1(obj),PTR2(obj));
    PTR2(obj) = aux; 
    PTR1(obj) = (void *) newtype;
    Tcl_InvalidateStringRep(obj);
}

Tcl_Obj *Tcl_PolyObjCancel(Tcl_Obj *obj, int mod) {
    if (Tcl_IsShared(obj)) 
        obj = Tcl_DuplicateObj(obj);
    PLcancel(PTR1(obj),PTR2(obj),mod);
    Tcl_InvalidateStringRep(obj);
    return obj;
}

Tcl_Obj *Tcl_PolyObjReflect(Tcl_Obj *obj) {
    polyType *tp = PTR1(obj);
    if (Tcl_IsShared(obj)) 
        obj = Tcl_DuplicateObj(obj);
    if (NULL == tp->reflect) 
        Tcl_PolyObjConvert(obj, (tp = stdpoly));
    (tp->reflect)(PTR2(obj));
    Tcl_InvalidateStringRep(obj);
    return obj;
}

Tcl_Obj *Tcl_PolyObjScaleMod(Tcl_Obj *obj, int scale, int mod) {
    polyType *tp = PTR1(obj);
    if (Tcl_IsShared(obj)) 
        obj = Tcl_DuplicateObj(obj);
    if (NULL == tp->scaleMod) 
        Tcl_PolyObjConvert(obj, (tp = stdpoly));
    (tp->scaleMod)(PTR2(obj),scale,mod);
    Tcl_InvalidateStringRep(obj);
    return obj;
}

Tcl_Obj *Tcl_PolyObjAppend(Tcl_Obj *obj, Tcl_Obj *pol2, int scale, int mod) {
    if (Tcl_IsShared(obj)) 
        ASSERT(NULL == "obj must not be shared in Tcl_PolyObjAppend");

    if (SUCCESS != PLappendPoly(PTR1(obj),PTR2(obj),PTR1(pol2),PTR2(pol2),NULL,0,scale,mod))
        return NULL;

    Tcl_InvalidateStringRep(obj);
    return obj;
}

Tcl_Obj *Tcl_PolyObjCompare(Tcl_Obj *a, Tcl_Obj *b) {
    int rval, rcode, ash, bsh;
    Tcl_Obj *ac = a, *bc = b;

    INCREFCNT(a);
    INCREFCNT(b);

    ash = Tcl_IsShared(a);
    bsh = Tcl_IsShared(b);

    if (SUCCESS == PLcompare(PTR1(ac),PTR2(ac),
                             PTR1(bc),PTR2(bc), 
                             &rval, (ash || bsh) ? 0 : PLF_ALLOWMODIFY)) {

        DECREFCNT(a);
        DECREFCNT(b);
        return Tcl_NewIntObj(rval);
    }

    DECREFCNT(a);
    DECREFCNT(b);

    /* need to modify the args, so have to use private copies */

    if (ash) ac = Tcl_DuplicateObj(a);
    if (bsh) bc = Tcl_DuplicateObj(b);
    
    rcode = PLcompare(PTR1(ac),PTR2(ac),PTR1(bc),PTR2(bc),&rval,PLF_ALLOWMODIFY);

    /* destroy private copies */
    if (a != ac) DECREFCNT(ac);
    if (b != bc) DECREFCNT(bc);

    return (SUCCESS == rcode) ? Tcl_NewIntObj(rval) : NULL;
}

Tcl_Obj *Tcl_PolyObjShift(Tcl_Obj *obj, exmo *e, int flags) {
    polyType *tp = PTR1(obj);

    ASSERTUNSHARED(obj, Tcl_PolyObjShift);

    if (NULL == tp->shift) 
        Tcl_PolyObjConvert(obj, (tp = stdpoly));

    (tp->shift)(PTR2(obj),e,flags);

    Tcl_InvalidateStringRep(obj);

    return obj;
}

Tcl_Obj *Tcl_PolyObjPosProduct(Tcl_Obj *obj, Tcl_Obj *pol2, int mod) {
    polyType *rtp; void *res;
    if (SUCCESS != PLposMultiply(&rtp,&res,
                                 PTR1(obj),PTR2(obj),
                                 PTR1(pol2),PTR2(pol2),mod))
        return NULL;
    return Tcl_NewPolyObj(rtp,res);
}

Tcl_Obj *Tcl_PolyObjNegProduct(Tcl_Obj *obj, Tcl_Obj *pol2, int mod) {
    polyType *rtp; void *res;
    if (SUCCESS != PLnegMultiply(&rtp,&res,
                                 PTR1(obj),PTR2(obj),
                                 PTR1(pol2),PTR2(pol2),mod))
        return NULL;
    return Tcl_NewPolyObj(rtp,res);
}

Tcl_Obj *Tcl_PolyObjSteenrodProduct(Tcl_Obj *obj, Tcl_Obj *pol2, primeInfo *pi) {
    polyType *rtp; void *res;
    if (SUCCESS != PLsteenrodMultiply(&rtp,&res,
                                      PTR1(obj),PTR2(obj),
                                      PTR1(pol2),PTR2(pol2),pi,NULL))
        return NULL;
    return Tcl_NewPolyObj(rtp,res);
}

Tcl_Obj *Tcl_PolyObjEBPProduct(Tcl_Obj *obj, Tcl_Obj *pol2, primeInfo *pi) {
    polyType *rtp; void *res;
    if (SUCCESS != PLEBPMultiply(&rtp,&res,
				 PTR1(obj),PTR2(obj),
				 PTR1(pol2),PTR2(pol2),pi))
        return NULL;
    return Tcl_NewPolyObj(rtp,res);
}

Tcl_Obj *Tcl_PolyObjGetCoeff(Tcl_Obj *obj, Tcl_Obj *exm, int mod) {
    int safeflags = 0, rval;

    INCREFCNT(obj);

    if (!Tcl_IsShared(obj)) safeflags |= PLF_ALLOWMODIFY;
    if (SUCCESS != PLcollectCoeffs(PTR1(obj),PTR2(obj),
                                   exmoFromTclObj(exm),&rval,mod,safeflags)) {
        DECREFCNT(obj);
        obj = Tcl_DuplicateObj(obj);   
        INCREFCNT(obj);
        
        if (SUCCESS != PLcollectCoeffs(PTR1(obj),PTR2(obj),
                                       exmoFromTclObj(exm),&rval,
                                       mod,PLF_ALLOWMODIFY)) {
            DECREFCNT(obj);
            return NULL;
        }
    }
    DECREFCNT(obj);
    return Tcl_NewIntObj(rval);
}

#define NEWSTRINGOBJ(strg) Tcl_NewStringObj(strg,strlen(strg))
Tcl_Obj *Tcl_PolyObjGetInfo(Tcl_Obj *obj) {
    polyInfo poli;
    char aux[500], *wrk=aux;;
    if (SUCCESS != PLgetInfo(PTR1(obj),PTR2(obj),&poli))
        return Tcl_NewObj();

    wrk += sprintf(wrk,"{implementation {%s}}"
                   " {{allocated bytes} %d}"
                   " {{bytes used} %d}"
                   " {{max reduced length} %d}",
                   poli.name ? poli.name : "unknown",
                   (unsigned) poli.bytesAllocated, (unsigned) poli.bytesUsed,
                   poli.maxRedLength);

    return NEWSTRINGOBJ(aux);
}        


int Tcl_PolyForeachProc(Tcl_Interp *ip, Tcl_Obj *src, 
                        Tcl_Obj *mvar, Tcl_Obj *script) {
    int rc = TCL_OK;
    polyType *pt; void *pdat; 
    int pns, idx;
    exmo mono;

    if (TCL_OK != Tcl_ConvertToPoly(ip, src))
        return TCL_ERROR;
        
    pt   = polyTypeFromTclObj(src);
    pdat = polyFromTclObj(src);
    pns  = PLgetNumsum(pt, pdat);

    for (idx=0; idx<pns; idx++) {
        
        if (SUCCESS != PLgetExmo(pt, pdat, &mono, idx)) {
            Tcl_SetResult(ip, "internal error in Tcl_PolyForeachProc: "
                          "PLgetExmo failed", TCL_STATIC);
            return TCL_ERROR;
        }

        if( NULL == Tcl_ObjSetVar2(ip,mvar,NULL,
                                   Tcl_NewExmoCopyObj(&mono),
                                   TCL_LEAVE_ERR_MSG) ) {
            return TCL_ERROR;
        }

        rc = Tcl_EvalObjEx(ip,script,0);
	if( rc == TCL_CONTINUE ) continue;
	if( rc == TCL_BREAK ) break;
        if( rc != TCL_OK ) return rc;
    }
    
    return TCL_OK;
}

/* The Tcl_PolySplitProc 
 *
 *  - iterates through the monomials in *src,
 *  - calls "*proc <mono>" for each monomial,
 * 
 * Let x be the result of this call. It determines the next action
 *      
 *   x < 0   : append this monomial to *res
 *   x >= 0  : append this monomial to "$(*objv[x])"
 *
 * If x is too big for the objv array the monomial is forgotten. */

int Tcl_PolySplitProc(Tcl_Interp *ip, int objc, Tcl_Obj *src, Tcl_Obj *proc, 
                      Tcl_Obj * CONST objv[], Tcl_Obj **res) {
    
    polyType *pt; void *pdat; 
    int pns, idx, i, x;
    exmo mono;
    Tcl_Obj **array; 
    void **parray;
    int rcode = SUCCESS, prc; 
    Tcl_Obj *(cmd[2]);

    if (TCL_OK != Tcl_ConvertToPoly(ip, src))
        return TCL_ERROR;

    /* First create an array of empty polynomials. We let array[k+1] 
     * correspond to objv[k] and array[0] to *res. */
    
    if (NULL == (array = mallox(sizeof(Tcl_Obj *) * (objc + 1))))
        RETERR("out of memory");
    
    if (NULL == (parray = mallox(sizeof(void *) * (objc + 1)))) {
        freex(array); 
        RETERR("out of memory");
    }

    for (i=0; i<(objc+1); i++) {
        if (NULL == (parray[i] = PLcreate(stdpoly))) {
            for (;i--;) DECREFCNT(array[i]); 
            freex(array); freex(parray);
            RETERR("out of memory");
        }
        array[i] = Tcl_NewPolyObj(stdpoly, parray[i]);
    }
    
    *res = array[0];

    /* Now assign array[i] to the variable objv[i]. Once we've done that
     * we need no longer worry about their refCounts. */

    for (i=0; i<objc; i++) 
        if (NULL == Tcl_ObjSetVar2(ip, objv[i], NULL,
                                   array[i+1], TCL_LEAVE_ERR_MSG)) {
            for (;i<objc;i++) 
                DECREFCNT(array[i+1]);
            DECREFCNT(array[0]);
            return TCL_ERROR;
        }
    

    pt   = polyTypeFromTclObj(src);
    pdat = polyFromTclObj(src);
    pns  = PLgetNumsum(pt, pdat);

    cmd[0] = proc; cmd[1] = NULL;

    for (idx=0; idx<pns; idx++) {
        
        if (SUCCESS != PLgetExmo(pt, pdat, &mono, idx)) {
            Tcl_SetResult(ip, "internal error in Tcl_PolySplitProc: "
                          "PLgetExmo failed", TCL_STATIC);
            rcode = TCL_ERROR;
            goto leave;
        }

        /* invoke filter proc */

        if (NULL != cmd[1]) DECREFCNT(cmd[1]); 
        cmd[1] = Tcl_NewExmoCopyObj(&mono); 
        INCREFCNT(cmd[1]);

        prc = Tcl_EvalObjv(ip, 2, cmd, 0); 

        if (TCL_OK != prc) {
            if (TCL_CONTINUE == prc) continue;
            if (TCL_BREAK    == prc) break;
            if (TCL_ERROR == prc) {
                Tcl_AddErrorInfo(ip, "\nwhile executing ");
                Tcl_AddErrorInfo(ip, Tcl_GetString(cmd[0]));
                Tcl_AddErrorInfo(ip, " {");
                Tcl_AddErrorInfo(ip, Tcl_GetString(cmd[1]));
                Tcl_AddErrorInfo(ip, "}");
                rcode = TCL_ERROR;
                goto leave;
            }
        }

        /* filter returned TCL_OK */

        if (TCL_OK != Tcl_GetIntFromObj(ip, Tcl_GetObjResult(ip), &x)) {
            Tcl_AddErrorInfo(ip, "\nwhile executing ");
            Tcl_AddErrorInfo(ip, Tcl_GetString(cmd[0]));
            Tcl_AddErrorInfo(ip, " {");
            Tcl_AddErrorInfo(ip, Tcl_GetString(cmd[1]));
            Tcl_AddErrorInfo(ip, "}");
            rcode = TCL_ERROR;
            goto leave;
        }

        if (x>=0) { 
            if (++x>objc) continue;
        } else x=0;

        /* append to parray[x] */
        
        if (SUCCESS != PLappendExmo(stdpoly, parray[x], &mono)) {
            Tcl_SetResult(ip, "internal error in Tcl_PolySplitProc: "
                          "PLappendExmo failed", TCL_STATIC);
            rcode = TCL_ERROR;
            goto leave;
        }
    }

 leave:
    if (NULL != cmd[1]) DECREFCNT(cmd[1]); 

    freex(array);
    freex(parray);

    if (SUCCESS != rcode) 
        DECREFCNT(*res);

    return rcode;
}

/* ---------- */

/* "TakePolyFromVar" treats its argument as the name of a variable which
 * is expected to contain a polynomial. This polynomial object is read,
 * an unshared copy is made whose refcount is incremented, the variable is
 * cleared, and the polynomial returned. If an error occurs a message is left
 * in the interpreter. If the variable doesn't exist it is automatically
 * created. */

Tcl_Obj *TakePolyFromVar(Tcl_Interp *ip, Tcl_Obj *varname) {
    Tcl_Obj *res;

    if (NULL == (res = Tcl_ObjGetVar2(ip, varname, NULL, TCL_LEAVE_ERR_MSG)))
        res = Tcl_NewPolyObj(stdpoly, PLcreate(stdpoly));

    if (TCL_OK != Tcl_ConvertToPoly(ip, res)) {
        Tcl_SetObjResult(ip, varname);
        Tcl_AppendResult(ip, " does not contain a valid polynomial", NULL);
        return NULL;
    }

    INCREFCNT(res);
    if (NULL == Tcl_ObjSetVar2(ip, varname, NULL, Tcl_NewObj(), TCL_LEAVE_ERR_MSG)) {
        DECREFCNT(res);
        return NULL;
    }

    if (Tcl_IsShared(res)) {
        DECREFCNT(res);
        res = Tcl_DuplicateObj(res);
        INCREFCNT(res);
    }

    return res;
}

#define EXPECTARGS(bas,min,max,msg) {                 \
  if ((objc<((bas)+(min))) || (objc>((bas)+(max)))) { \
       Tcl_WrongNumArgs(ip, (bas), objv, msg);        \
       return TCL_ERROR; } }

/**** Implementation of the poly combi-command ********************************/

typedef enum { CREATE, TEST, INFO, APPEND, CANCEL, ADD, POSMULT, NEGMULT, 
               STEENMULT, VARAPPEND, VARCANCEL, SHIFT, REFLECT, 
               COMPARE, SPLIT, VARSPLIT, COEFF, FOREACH, EBPMULT } pcmdcode;

static CONST char *pCmdNames[] = { "create", "test", "info", "append", "cancel", 
                                   "add", "posmult", "negmult", "steenmult",
                                   "varappend", "varcancel", "shift", "reflect",
                                   "compare", "split", "varsplit", "coeff",
                                   "foreach", "ebpmult",
                                   (char *) NULL };

static pcmdcode pCmdmap[] = { CREATE, TEST, INFO, APPEND, CANCEL, ADD,
                              POSMULT, NEGMULT, STEENMULT, VARAPPEND, VARCANCEL, 
                              SHIFT, REFLECT, COMPARE, SPLIT, VARSPLIT, COEFF, 
                              FOREACH, EBPMULT };

int PolyCombiCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[]) {
    int result, index, scale, modval;
    primeInfo *pi;
    exmo *ex;
    Tcl_Obj *(varp[5]), *obj1, *obj;
    
    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }
    
    result = Tcl_GetIndexFromObj(ip, objv[1], pCmdNames, "subcommand", 0, &index);
    if (result != TCL_OK) return result;

    switch (pCmdmap[index]) {
        case CREATE:
            EXPECTARGS(2, 0, 0, NULL);
            
            Tcl_SetObjResult(ip, Tcl_NewPolyObj(stdpoly, PLcreate(stdpoly)));
            return TCL_OK;

        case TEST:
            EXPECTARGS(2, 1, 1, "<polynomial candidate>");

            Tcl_ConvertToPoly(ip, objv[2]);
            return TCL_OK;

        case INFO:
            EXPECTARGS(2, 1, 1, "<polynomial>");

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            Tcl_SetObjResult(ip, Tcl_PolyObjGetInfo(objv[2]));
            return TCL_OK;

        case APPEND:
            EXPECTARGS(2, 2, 4, "<polynomial> <polynomial> ?<scale>? ?<mod>?"); 

            scale = 1; modval = 0;
            if (objc > 4)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &scale))
                    return TCL_ERROR;

            if (objc > 5)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[5], &modval))
                    return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[3]))
                return TCL_ERROR;
            
            /* TODO: check if we should increment the refcount first! */

            obj = objv[2];
            if (Tcl_IsShared(obj)) 
                obj = Tcl_DuplicateObj(obj);

            if (NULL == (obj1 = Tcl_PolyObjAppend(obj, objv[3], scale, modval)))
                RETERR("PLappendPoly failed");

            ASSERT(obj == obj1);

            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;
            
        case CANCEL:
            EXPECTARGS(2, 1, 2, "<polynomial> ?<integer>?"); 

            modval = 0;
            if (objc > 3)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &modval))
                    return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            Tcl_SetObjResult(ip, Tcl_PolyObjCancel(objv[2], modval));
            return TCL_OK;

        case ADD:
            EXPECTARGS(2, 2, 4, "<polynomial> <polynomial> ?<scale>? ?<mod>?"); 

            scale = 1; modval = 0;
            if (objc > 4)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &scale))
                    return TCL_ERROR;

            if (objc > 5)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[5], &modval))
                    return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[3]))
                return TCL_ERROR;
            
            obj = objv[2];
            if (Tcl_IsShared(obj)) 
                obj = Tcl_DuplicateObj(obj);
            if (NULL == (obj1 = Tcl_PolyObjAppend(obj, objv[3], scale, modval)))
                RETERR("PLappendPoly failed");

            ASSERT(obj == obj1);

            Tcl_SetObjResult(ip, Tcl_PolyObjCancel(obj1, modval));
            return TCL_OK;  
             
        case POSMULT:
            EXPECTARGS(2, 2, 3, "<polynomial> <polynomial> ?<mod>?"); 

            modval = 0;
            if (objc > 4)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &modval))
                    return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[3]))
                return TCL_ERROR;

            if (NULL == (obj1 = Tcl_PolyObjPosProduct(objv[2], objv[3], modval)))
                RETERR("Tcl_PolyObjPosProduct failed");

            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;  

        case NEGMULT:
            EXPECTARGS(2, 2, 3, "<polynomial> <polynomial> ?<mod>?"); 

            modval = 0;
            if (objc > 4)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &modval))
                    return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[3]))
                return TCL_ERROR;

            if (NULL == (obj1 = Tcl_PolyObjNegProduct(objv[2], objv[3], modval)))
                RETERR("Tcl_PolyObjNegProduct failed");

            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;  

        case STEENMULT:
            EXPECTARGS(2, 3, 3, "<polynomial> <polynomial> <prime>"); 

            if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[4], &pi))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[3]))
                return TCL_ERROR;

            if (NULL == (obj1 = Tcl_PolyObjSteenrodProduct(objv[2], objv[3], pi)))
                RETERR("Tcl_PolyObjSteenrodProduct failed");

            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;  

        case EBPMULT:
            EXPECTARGS(2, 3, 3, "<polynomial> <polynomial> <prime>"); 

            if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[4], &pi))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[3]))
                return TCL_ERROR;

            if (NULL == (obj1 = Tcl_PolyObjEBPProduct(objv[2], objv[3], pi)))
                RETERR("Tcl_PolyObjSteenrodProduct failed");

            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;  

        case VARAPPEND:
            EXPECTARGS(2, 2, 4, "<variable> <polynomial> ?<scale>? ?<mod>?"); 

            scale = 1; modval = 0;
            if (objc > 4)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &scale))
                    return TCL_ERROR;

            if (objc > 5)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[5], &modval))
                    return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[3]))
                return TCL_ERROR;

            if (NULL == (varp[1] = TakePolyFromVar(ip, objv[2])))
                return TCL_ERROR;

            if (NULL == (obj1 = Tcl_PolyObjAppend(varp[1], objv[3], scale, modval)))
                RETERR("PLappendPoly failed");

            ASSERT(obj1 == varp[1]);

            if (NULL == Tcl_ObjSetVar2(ip, objv[2], NULL, obj1, TCL_LEAVE_ERR_MSG)) {
                DECREFCNT(varp[1]);
                return TCL_ERROR;
            }
            
            DECREFCNT(varp[1]);
            return TCL_OK;
            
        case VARCANCEL:
            EXPECTARGS(2, 1, 2, "<variable> ?<integer>?"); 

            modval = 0;
            if (objc > 3)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &modval))
                    return TCL_ERROR;

            if (NULL == (varp[1] = TakePolyFromVar(ip, objv[2])))
                return TCL_ERROR;

            obj1 = Tcl_PolyObjCancel(varp[1], modval);
            
            /* since varp[1] is unshared, we should have obj1 == varp[1] */

            ASSERT(obj1 == varp[1]);

            if (NULL == Tcl_ObjSetVar2(ip, objv[2], NULL, varp[1], TCL_LEAVE_ERR_MSG)) {
                DECREFCNT(varp[1]);
                return TCL_ERROR;
            }
            
            DECREFCNT(varp[1]);
            return TCL_OK;

        case SPLIT:
            EXPECTARGS(2, 2, 666, "<polynomial> <filter proc> ?var0? ?var1? ..."); 

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_PolySplitProc(ip, objc-4, objv[2],
                                            objv[3], objv+4, &obj1))
                return TCL_ERROR;
            
            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;
            
        case FOREACH:
            EXPECTARGS(2, 3, 3, "<polynomial> <monovar> <script>"); 

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_PolyForeachProc(ip, objv[2], objv[3], objv[4]))
                return TCL_ERROR;
            
            return TCL_OK;
            
        case VARSPLIT:
            EXPECTARGS(2, 2, 666, "<variable> <filter proc> ?var0? ?var1? ..."); 

            if (NULL == (varp[1] = TakePolyFromVar(ip, objv[2])))
                return TCL_ERROR;

            if (TCL_OK != Tcl_PolySplitProc(ip, objc-4, varp[1],
                                            objv[3], objv+4, &obj1)) {
                DECREFCNT(varp[1]);
                return TCL_ERROR;
            }

            if (NULL == Tcl_ObjSetVar2(ip, objv[2], NULL, obj1, TCL_LEAVE_ERR_MSG)) {
                DECREFCNT(varp[1]);
                return TCL_ERROR;
            }
 
            DECREFCNT(varp[1]);
            return TCL_OK;
           
        case SHIFT:
            EXPECTARGS(2, 2, 3, "<polynomial> <monomial> ?<boolean: with signs>?");

            modval = 0;
            
            if (objc > 4)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &modval))
                    return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[3]))
                return TCL_ERROR;

            ex = exmoFromTclObj(objv[3]);

            obj = objv[2];

            INCREFCNT(obj);
            if (Tcl_IsShared(obj)) {
                DECREFCNT(obj);
                obj = Tcl_DuplicateObj(obj);
                INCREFCNT(obj);
            }

            Tcl_SetObjResult(ip, Tcl_PolyObjShift(obj, ex, modval));
            
            DECREFCNT(obj);
            return TCL_OK;

        case REFLECT:
            EXPECTARGS(2, 1, 1, "<polynomial>");

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;
            
            Tcl_SetObjResult(ip, Tcl_PolyObjReflect(objv[2]));
            return TCL_OK;

        case COMPARE:
            EXPECTARGS(2, 2, 2, "<polynomial> <polynomial>");

            if (objv[2] == objv[3]) {
                Tcl_SetObjResult(ip, Tcl_NewIntObj(0));
                return TCL_OK;
            }

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;
            
            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[3]))
                return TCL_ERROR;
            
            if (NULL == (obj1 = Tcl_PolyObjCompare(objv[2],objv[3])))
                RETERR("comparison not possible");
            
            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;

        case COEFF:
            EXPECTARGS(2, 2, 2, "<polynomial> <monomial>");

            if (TCL_OK != Tcl_ConvertToPoly(ip, objv[2]))
                return TCL_ERROR;
            
            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[3]))
                return TCL_ERROR;
            
            if (NULL == (obj1 = Tcl_PolyObjGetCoeff(objv[2], objv[3], 0)))
                RETERR("PLcollectCoeff failed");

            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;
    }
    
    Tcl_SetResult(ip, "internal error in PolyCombiCmd", TCL_STATIC);
    return TCL_ERROR;
}

/**** Implementation of the mono combi-command ********************************/

typedef enum { MTEST, ISABOVE, ISBELOW, MCOMPARE, LENGTH, RLENGTH, PADDING, 
               GEN, MCOEFF, MEXT, MEXP, MDEG, MRDEG, MEDEG } mcmdcode;

static CONST char *mCmdNames[] = { "test", "isabove", "isbelow", "compare",
                                   "length", "rlength", "padding",
                                   "gen", "coeff", "exterior", "exponent",
                                   "degree", "rdegree", "edegree",
                                   (char *) NULL };

static mcmdcode mCmdmap[] = { MTEST, ISABOVE, ISBELOW, MCOMPARE, LENGTH, RLENGTH, PADDING, 
                              GEN, MCOEFF, MEXT, MEXP, MDEG, MRDEG, MEDEG };

int MonoCombiCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[]) {
    int result, index;
    primeInfo *pi;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(ip, objv[1], mCmdNames, "subcommand", 0, &index);
    if (result != TCL_OK) return result;

    switch (mCmdmap[index]) {
        case MTEST:
            EXPECTARGS(2, 1, 1, "<monomial candidate>");

            Tcl_ConvertToExmo(ip, objv[2]);
            return TCL_OK;

        case GEN:
            EXPECTARGS(2, 1, 1, "<monomial>");

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
        return TCL_ERROR;
        
        Tcl_SetObjResult(ip, Tcl_NewIntObj(exmoFromTclObj(objv[2])->gen));
            return TCL_OK;

        case MCOEFF:
            EXPECTARGS(2, 1, 1, "<monomial>");

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
        return TCL_ERROR;
        
        Tcl_SetObjResult(ip, Tcl_NewIntObj(exmoFromTclObj(objv[2])->coeff));
            return TCL_OK;

        case MEXT:
            EXPECTARGS(2, 1, 1, "<monomial>");

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
        return TCL_ERROR;
        
        Tcl_SetObjResult(ip, Tcl_NewIntObj(exmoFromTclObj(objv[2])->ext));
            return TCL_OK;

        case MEDEG:
            EXPECTARGS(2, 1, 1, "<monomial>");

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
        return TCL_ERROR;
        
        Tcl_SetObjResult(ip, Tcl_NewIntObj(BITCOUNT(exmoFromTclObj(objv[2])->ext)));
            return TCL_OK;

        case MDEG:
            EXPECTARGS(2, 2, 2, "<prime> <monomial>");

            if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[2], &pi))
                return TCL_ERROR;
            
            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[3]))
        return TCL_ERROR;
        
            Tcl_SetObjResult(ip, Tcl_NewIntObj(exmoIdeg(pi,exmoFromTclObj(objv[3]))));
            return TCL_OK;

        case MRDEG:
            EXPECTARGS(2, 2, 2, "<prime> <monomial>");

            if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[2], &pi))
                return TCL_ERROR;
            
            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[3]))
        return TCL_ERROR;
        
            Tcl_SetObjResult(ip, Tcl_NewIntObj(exmoRdeg(pi,exmoFromTclObj(objv[3]))));
            return TCL_OK;

        case MEXP:
            EXPECTARGS(2, 2, 2, "<monomial> <index>");

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
        return TCL_ERROR;
        
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &index))
        return TCL_ERROR;
        
        if (index < 0) {
        Tcl_SetResult(ip, "index must be nonnegative", TCL_STATIC);
        return TCL_ERROR;
        }

        if (index >= NALG) {
        result = exmoGetPad(exmoFromTclObj(objv[2]));
        } else {
        result = exmoFromTclObj(objv[2])->r.dat[index];
        }
        
        Tcl_SetObjResult(ip, Tcl_NewIntObj(result));
            return TCL_OK;

        case ISABOVE:
            EXPECTARGS(2, 2, 2, "<monomial> <monomial>");
            
            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[3]))
                return TCL_ERROR;

            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 exmoIsAbove(exmoFromTclObj(objv[2]),
                                             exmoFromTclObj(objv[3]))));
            return TCL_OK;

        case MCOMPARE:
            EXPECTARGS(2, 2, 2, "<monomial> <monomial>");
            
            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[3]))
                return TCL_ERROR;

            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 compareExmo(exmoFromTclObj(objv[2]),
                                             exmoFromTclObj(objv[3]))));
            return TCL_OK;

        case ISBELOW:
            EXPECTARGS(2, 2, 2, "<monomial> <monomial>");
            
            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[3]))
                return TCL_ERROR;

            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 exmoIsBelow(exmoFromTclObj(objv[2]),
                                             exmoFromTclObj(objv[3]))));
            return TCL_OK;

        case LENGTH:
            EXPECTARGS(2, 1, 1, "<monomial>");

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
                return TCL_ERROR;

            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 exmoGetLen(exmoFromTclObj(objv[2]))));
            return TCL_OK;

        case RLENGTH:
            EXPECTARGS(2, 1, 1, "<monomial>");

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
                return TCL_ERROR;

            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 exmoGetRedLen(exmoFromTclObj(objv[2]))));
            return TCL_OK;

        case PADDING:
            EXPECTARGS(2, 1, 1, "<monomial>");

            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
                return TCL_ERROR;

            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 exmoGetPad(exmoFromTclObj(objv[2]))));
            return TCL_OK;
    }
    
    Tcl_SetResult(ip, "internal error in MonoCombiCmd", TCL_STATIC);
    return TCL_ERROR;
}


#define CREATECMD(name, id) \
  Tcl_CreateObjCommand(ip, name, tPolyCombiCmd, \
                       (ClientData) id, NULL)

int Tpoly_HaveTypes = 0;

int Tpoly_Init(Tcl_Interp *ip) {
    
    Tcl_InitStubs(ip, "8.0", 0);

    Tptr_Init(ip);
    Tprime_Init(ip);

    if (!Tpoly_HaveTypes) {
        /* set up types and register */ 
        tclExmo.name               = "monomial";
        tclExmo.freeIntRepProc     = ExmoFreeInternalRepProc;
        tclExmo.dupIntRepProc      = ExmoDupInternalRepProc;
        tclExmo.updateStringProc   = ExmoUpdateStringProc;
        tclExmo.setFromAnyProc     = ExmoSetFromAnyProc;
        Tcl_RegisterObjType(&tclExmo);
        TPtr_RegObjType(TP_EXMO, &tclExmo);
        
        /* set up types and register */ 
        tclPoly.name               = "polynomial";
        tclPoly.freeIntRepProc     = PolyFreeInternalRepProc;
        tclPoly.dupIntRepProc      = PolyDupInternalRepProc;
        tclPoly.updateStringProc   = PolyUpdateStringProc;
        tclPoly.setFromAnyProc     = PolySetFromAnyProc;
        Tcl_RegisterObjType(&tclPoly);
        TPtr_RegObjType(TP_POLY, &tclPoly);
       
        Tpoly_HaveTypes = 1;
    }

    Tcl_CreateObjCommand(ip, POLYNSP "poly", PolyCombiCmd, (ClientData) 0, NULL);
    Tcl_CreateObjCommand(ip, POLYNSP "mono", MonoCombiCmd, (ClientData) 0, NULL);

    Tcl_LinkVar(ip, POLYNSP "_multCount", (char *) &multCount, TCL_LINK_INT);

    Tcl_UnlinkVar(ip, POLYNSP "_polCount"); 
    Tcl_LinkVar(ip, POLYNSP "_polCount", (char *) &polCount, 
                TCL_LINK_INT | TCL_LINK_READ_ONLY);

    Tcl_UnlinkVar(ip, POLYNSP "_monCount"); 
    Tcl_LinkVar(ip, POLYNSP "_monCount", (char *) &monCount, 
                TCL_LINK_INT | TCL_LINK_READ_ONLY);

    return TCL_OK;
}
