/*
 * Tcl interface to the polynomial routines
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

#define DBGPOLY if (0)

#include "tptr.h"
#include "tpoly.h"
#include "tprime.h"
#include "poly.h"
#include "mult.h"

#include <string.h>

#define FREETCLOBJ(obj) { Tcl_IncrRefCount(obj); Tcl_DecrRefCount(obj); }

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
    if (TCL_OK != Tcl_GetIntFromObj(ip,objv[0],&(e->coeff))) 
        FREEEANDRETERR;
    if (TCL_OK != Tcl_GetIntFromObj(ip,objv[1],&(e->ext))) 
        FREEEANDRETERR;
    if (TCL_OK != Tcl_GetIntFromObj(ip,objv[3],&(e->gen))) 
        FREEEANDRETERR;
    if (TCL_OK != Tcl_ListObjGetElements(ip, objv[2], &objc2, &objv2))
        return TCL_ERROR;
    if (objc2 > NALG) 
        RETERR("exponent sequence too long");
    if (e->ext < 0) pad = -1;
    for (i=0;i<objc2;i++) {
        if (TCL_OK != Tcl_GetIntFromObj(ip,objv2[i],&aux)) 
            FREEEANDRETERR;
        if (aux<0) pad = -1;
        e->dat[i] = aux;
    }
    for (;i<NALG;) e->dat[i++] = pad;

    TRYFREEOLDREP(objPtr);
    PTR1(objPtr) = e;
    objPtr->typePtr = &tclExmo;

    return TCL_OK;
}

/* Create a new list Obj from the objPtr */
Tcl_Obj *Tcl_NewListFromExmo(Tcl_Obj *objPtr) {
    exmo *e = (exmo *) PTR1(objPtr);
    Tcl_Obj *res, *(objv[4]), **arr; 
    int i, len = exmoGetRedLen(e);
    arr = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *)*len);
    for (i=0;i<len;i++) arr[i] = Tcl_NewIntObj(e->dat[i]);
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
}

/**************************************************************************
 *
 * The Tcl type for generic polynomials. To the Tcl user such a thing 
 * behaves like a list of extended monomials. 
 *
 * The implementation lets PTR1 point to the polyType and PTR2 to the data. 
 */

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
}

/**** Tcl wrappers for some of the PL functions */

/* for some functions we need to convert the type first */
void Tcl_PolyObjConvert(Tcl_Obj *obj, polyType *newtype) {
    void *aux;
    DBGPOLY printf("Tcl_PolyObjConvert obj = %p\n", obj);
    if (PTR1(obj) == newtype) return;
    if (Tcl_IsShared(obj))
        assert(NULL == "Tcl_PolyObjConvert called for shared object");
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

Tcl_Obj *Tcl_PolyObjAppend(Tcl_Obj *obj, Tcl_Obj *pol2) {
    Tcl_IncrRefCount(obj);
    if (Tcl_IsShared(obj)) {    
        Tcl_DecrRefCount(obj);
        obj = Tcl_DuplicateObj(obj);    
        Tcl_IncrRefCount(obj);
    }
    if (SUCCESS != PLappendPoly(PTR1(obj),PTR2(obj),PTR1(pol2),PTR2(pol2),NULL,0,1,0))
        return NULL;
    Tcl_InvalidateStringRep(obj);
    return obj;
}

Tcl_Obj *Tcl_PolyObjCompare(Tcl_Obj *a, Tcl_Obj *b) {
    int rval, rcode, ash, bsh;
    Tcl_Obj *ac = a, *bc = b;

    Tcl_IncrRefCount(a);
    Tcl_IncrRefCount(b);

    ash = Tcl_IsShared(a);
    bsh = Tcl_IsShared(b);

    Tcl_DecrRefCount(a);
    Tcl_DecrRefCount(b);

    if (SUCCESS == PLcompare(PTR1(ac),PTR2(ac),
                             PTR1(bc),PTR2(bc), 
                             &rval, (ash || bsh) ? 0 : PLF_ALLOWMODIFY)) {
        return Tcl_NewIntObj(rval);
    }

    /* need to modify the args, so have to use private copies */

    if (ash) ac = Tcl_DuplicateObj(a);
    if (bsh) bc = Tcl_DuplicateObj(b);
    
    rcode = PLcompare(PTR1(ac),PTR2(ac),PTR1(bc),PTR2(bc),&rval,PLF_ALLOWMODIFY);

    /* destroy private copies */
    if (a != ac) Tcl_DecrRefCount(ac);
    if (b != bc) Tcl_DecrRefCount(bc);

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

Tcl_Obj *Tcl_PolyObjGetCoeff(Tcl_Obj *obj, Tcl_Obj *exm, int mod) {
    int safeflags = 0, rval;

    Tcl_IncrRefCount(obj);

    if (!Tcl_IsShared(obj)) safeflags |= PLF_ALLOWMODIFY;
    if (SUCCESS != PLcollectCoeffs(PTR1(obj),PTR2(obj),
                                   exmoFromTclObj(exm),&rval,mod,safeflags)) {
        Tcl_DecrRefCount(obj);
        obj = Tcl_DuplicateObj(obj);   
        Tcl_IncrRefCount(obj);
        
        if (SUCCESS != PLcollectCoeffs(PTR1(obj),PTR2(obj),
                                       exmoFromTclObj(exm),&rval,
                                       mod,PLF_ALLOWMODIFY)) {
            Tcl_DecrRefCount(obj);
            return NULL;
        }
    }
    Tcl_DecrRefCount(obj);
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
            for (;i--;) Tcl_DecrRefCount(array[i]); 
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
                Tcl_DecrRefCount(array[i+1]);
            Tcl_DecrRefCount(array[0]);
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

        if (NULL != cmd[1]) Tcl_DecrRefCount(cmd[1]); 
        cmd[1] = Tcl_NewExmoCopyObj(&mono); 
        Tcl_IncrRefCount(cmd[1]);

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
    if (NULL != cmd[1]) Tcl_DecrRefCount(cmd[1]); 

    freex(array);
    freex(parray);

    if (SUCCESS != rcode) 
        Tcl_DecrRefCount(*res);

    return rcode;
}

/**** The Combi Command */

typedef enum { 
    TPUNSHARE, TPEXMO, TPPOLY, 
    TPINFO, TPGETCOEFF,
    TPCANCEL, TPSHIFT, TPREFLECT, TPSCALE, TPAPPEND, TPCOMPARE,
    TPNEGMULT, TPPOSMULT, TPSTMULT, TPSPLIT, TPVARSPLIT,
    TMABOVE, TMBELOW, TMLENGTH, TMRLENGTH, TMPAD
} PolyCmdCode;

int tPolyCombiCmd(ClientData cd, Tcl_Interp *ip, 
                  int objc, Tcl_Obj * CONST objv[]) {
    PolyCmdCode cdi = (PolyCmdCode) cd;
    int ivar, ivar2;
    Tcl_Obj *obj, *obj1;
    primeInfo *pi;

    switch (cdi) {
        case TPUNSHARE:
            ENSUREARGS1(TP_ANY);
            obj = objv[1];
            if (Tcl_IsShared(obj)) 
                obj = Tcl_DuplicateObj(obj);
            Tcl_SetObjResult(ip,obj);
            return TCL_OK;
        case TPEXMO:
            ENSUREARGS1(TP_EXMO);
            Tcl_InvalidateStringRep(objv[1]);
            Tcl_SetObjResult(ip, objv[1]);
            return TCL_OK;
        case TPPOLY:
            ENSUREARGS1(TP_POLY);
            Tcl_InvalidateStringRep(objv[1]);
            Tcl_SetObjResult(ip, objv[1]);
            return TCL_OK;
        case TPSCALE:
            ENSUREARGS4(TP_POLY,TP_INT,TP_OPTIONAL,TP_INT);
            Tcl_GetIntFromObj(ip, objv[2], &ivar);
            if (4 == objc) 
                Tcl_GetIntFromObj(ip, objv[3], &ivar2);
            else 
                ivar2 = 0;
            Tcl_SetObjResult(ip, Tcl_PolyObjScaleMod(objv[1], ivar, ivar2));
            return TCL_OK;
        case TPSHIFT:
            ENSUREARGS4(TP_POLY,TP_EXMO,TP_OPTIONAL,TP_INT);
            if (4 == objc) 
                Tcl_GetIntFromObj(ip, objv[3], &ivar);
            else 
                ivar = 0;

            obj = objv[1];

            Tcl_IncrRefCount(obj);
            if (Tcl_IsShared(obj)) {
                Tcl_DecrRefCount(obj);
                obj = Tcl_DuplicateObj(obj);
                Tcl_IncrRefCount(obj);
            }
    
            Tcl_SetObjResult(ip, Tcl_PolyObjShift(obj, 
                                                  exmoFromTclObj(objv[2]), ivar));

            Tcl_DecrRefCount(obj);
            return TCL_OK;
        case TPCANCEL:
            ENSUREARGS3(TP_POLY,TP_OPTIONAL,TP_INT);
            if (3 == objc) 
                Tcl_GetIntFromObj(ip, objv[2], &ivar);
            else 
                ivar = 0;
            Tcl_SetObjResult(ip, Tcl_PolyObjCancel(objv[1], ivar));
            return TCL_OK;
        case TPREFLECT:
            ENSUREARGS1(TP_POLY);
            Tcl_SetObjResult(ip, Tcl_PolyObjReflect(objv[1]));
            return TCL_OK;
        case TPAPPEND:
            ENSUREARGS2(TP_POLY,TP_POLY);
            if (NULL == (obj1 = Tcl_PolyObjAppend(objv[1], objv[2])))
                RETERR("PLappendPoly failed");
            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;
        case TPCOMPARE:
            ENSUREARGS2(TP_POLY,TP_POLY);
            if (NULL == (obj1 = Tcl_PolyObjCompare(objv[1],objv[2])))
                RETERR("comparison not possible");
            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;
        case TPPOSMULT:
            ENSUREARGS4(TP_POLY,TP_POLY,TP_OPTIONAL,TP_INT);
            if (4 == objc) 
                Tcl_GetIntFromObj(ip, objv[3], &ivar);
            else 
                ivar = 0;
            if (NULL == (obj1 = Tcl_PolyObjPosProduct(objv[1], objv[2], ivar)))
                RETERR("PLposMultiply failed");
            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;            
        case TPNEGMULT:
            ENSUREARGS4(TP_POLY,TP_POLY,TP_OPTIONAL,TP_INT);
            if (4 == objc) 
                Tcl_GetIntFromObj(ip, objv[3], &ivar);
            else 
                ivar = 0;
            if (NULL == (obj1 = Tcl_PolyObjNegProduct(objv[1], objv[2], ivar)))
                RETERR("PLnegMultiply failed");
            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;            
        case TPSTMULT:
            ENSUREARGS3(TP_PRIME,TP_POLY,TP_POLY);
            if (TCL_OK != Tcl_GetPrimeInfo(ip,objv[1],&pi))
                return TCL_ERROR;
            if (NULL == (obj1 = Tcl_PolyObjSteenrodProduct(objv[2], objv[3], pi)))
                RETERR("PLsteenrodMultiply failed");
            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;            
        case TPINFO:
            ENSUREARGS1(TP_POLY);
            Tcl_SetObjResult(ip, Tcl_PolyObjGetInfo(objv[1]));
            return TCL_OK;
        case TPGETCOEFF:
            ENSUREARGS4(TP_POLY,TP_EXMO,TP_OPTIONAL,TP_INT);
            if (4 == objc) 
                Tcl_GetIntFromObj(ip, objv[3], &ivar);
            else 
                ivar = 0;
            if (NULL == (obj1 = Tcl_PolyObjGetCoeff(objv[1], objv[2], ivar)))
                RETERR("PLcollectCoeff failed");
            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;            
        case TPSPLIT:
            if (objc < 3) {
                Tcl_WrongNumArgs(ip, 1, objv, 
                                 "<polynomial> <filter proc> ?var0? ?var1? ...");
                return TCL_ERROR;
            }

            ENSUREARGS3(TP_POLY,TP_PROCNAME,TP_VARARGS);

            if (TCL_OK != Tcl_PolySplitProc(ip, objc-3, objv[1], 
                                            objv[2], objv+3, &obj1)) 
                return TCL_ERROR;

            Tcl_SetObjResult(ip, obj1);
            return TCL_OK;

        case TPVARSPLIT:
            if (objc < 3) {
                Tcl_WrongNumArgs(ip, 1, objv, 
                                 "<variable> <filter proc> ?var0? ?var1? ...");
                return TCL_ERROR;
            }
            ENSUREARGS3(TP_VARNAME,TP_PROCNAME,TP_VARARGS);
            
            obj = Tcl_ObjGetVar2(ip, objv[1], NULL, TCL_LEAVE_ERR_MSG);
            if (NULL == obj) return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToPoly(ip, obj))
                return TCL_ERROR;
            
            Tcl_IncrRefCount(obj);
            if (NULL == Tcl_ObjSetVar2(ip, objv[1], NULL,
                                       Tcl_NewObj(), TCL_LEAVE_ERR_MSG)) {
                Tcl_DecrRefCount(obj);
                return TCL_ERROR;
            }

            if (Tcl_IsShared(obj)) {
                Tcl_DecrRefCount(obj);
                obj = Tcl_DuplicateObj(obj);
                Tcl_IncrRefCount(obj);
            }

            if (TCL_OK != Tcl_PolySplitProc(ip, objc-3, obj, objv[2], 
                                            objv+3, &obj1)) {
                Tcl_DecrRefCount(obj);
                return TCL_ERROR;
            }
            
            if (NULL == Tcl_ObjSetVar2(ip, objv[1], NULL,
                                       obj1, TCL_LEAVE_ERR_MSG)) {
                return TCL_ERROR;
            }

            Tcl_DecrRefCount(obj);
            
            Tcl_ResetResult(ip);
            return TCL_OK;
            
        case TMABOVE:
            ENSUREARGS2(TP_EXMO, TP_EXMO);
            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 exmoIsAbove(exmoFromTclObj(objv[1]),
                                             exmoFromTclObj(objv[2]))));
            return TCL_OK;
            
        case TMBELOW:
            ENSUREARGS2(TP_EXMO, TP_EXMO);
            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 exmoIsBelow(exmoFromTclObj(objv[1]),
                                             exmoFromTclObj(objv[2]))));
            return TCL_OK;

        case TMLENGTH:
            ENSUREARGS1(TP_EXMO);
            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 exmoGetLen(exmoFromTclObj(objv[1]))));
            return TCL_OK;

        case TMRLENGTH:
            ENSUREARGS1(TP_EXMO);
            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 exmoGetRedLen(exmoFromTclObj(objv[1]))));
            return TCL_OK;

        case TMPAD:
            ENSUREARGS1(TP_EXMO);
            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 exmoGetPad(exmoFromTclObj(objv[1]))));
            return TCL_OK;

    }

    RETERR("tPolyCombiCmd: internal error");
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

    CREATECMD(POLYNSP "unshare",     TPUNSHARE);

    CREATECMD(POLYNSP "exmocheck",   TPEXMO);
    CREATECMD(POLYNSP "polycheck",   TPPOLY);

    CREATECMD(POLYNSP "shift",   TPSHIFT);
    CREATECMD(POLYNSP "reflect", TPREFLECT);
    CREATECMD(POLYNSP "scale",   TPSCALE);
    CREATECMD(POLYNSP "append",  TPAPPEND);
    CREATECMD(POLYNSP "compare", TPCOMPARE);
    CREATECMD(POLYNSP "cancel",  TPCANCEL);
    CREATECMD(POLYNSP "negmult", TPNEGMULT);
    CREATECMD(POLYNSP "posmult", TPPOSMULT);
    CREATECMD(POLYNSP "stmult",  TPSTMULT);

    CREATECMD(POLYNSP "split",    TPSPLIT);
    CREATECMD(POLYNSP "varsplit", TPVARSPLIT);

    CREATECMD(POLYNSP "coeff",  TPGETCOEFF);
    CREATECMD(POLYNSP "info",   TPINFO);

    CREATECMD(MONONSP "isabove",   TMABOVE);
    CREATECMD(MONONSP "isbelow",   TMBELOW);
    CREATECMD(MONONSP "length",    TMLENGTH);
    CREATECMD(MONONSP "redlength", TMRLENGTH);
    CREATECMD(MONONSP "padding",   TMPAD);

    Tcl_LinkVar(ip, POLYNSP "multCount", (char *) &multCount, TCL_LINK_INT);

    return TCL_OK;
}
