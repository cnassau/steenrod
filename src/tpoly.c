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

#include "tptr.h"
#include "tpoly.h"
#include "tprime.h"
#include "poly.h"

#include <string.h>

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
    exmo *x = malloc(sizeof(exmo));
    TCLMEMASSERT(x);
    copyExmo(x,ex);
    return Tcl_NewExmoObj(x);
}

/* free internal representation */
void ExmoFreeInternalRepProc(Tcl_Obj *obj) {
    free(PTR1(obj));
}

#define FREEEANDRETERR { free((char *) e); return TCL_ERROR; }

/* try to turn objPtr into an Exmo */
int ExmoSetFromAnyProc(Tcl_Interp *ip, Tcl_Obj *objPtr) {
    int objc, objc2, i, pad=0, aux;
    Tcl_Obj **objv, **objv2;
    exmo *e;
    if (TCL_OK != Tcl_ListObjGetElements(ip, objPtr, &objc, &objv))
        return TCL_ERROR;
    if (4 != objc) 
        RETERR("malformed monomial: wrong number of entries");
    if (TCL_OK != Tcl_ListObjGetElements(ip, objv[2], &objc2, &objv2))
        return TCL_ERROR;
    if (objc2 > NALG) 
        RETERR("exponent sequence too long");
    if (NULL == (e = (exmo *) malloc(sizeof(exmo))))
        RETERR("out of memory");
    if (TCL_OK != Tcl_GetIntFromObj(ip,objv[0],&(e->coeff))) 
        FREEEANDRETERR;
    if (TCL_OK != Tcl_GetIntFromObj(ip,objv[1],&(e->ext))) 
        FREEEANDRETERR;
    if (TCL_OK != Tcl_GetIntFromObj(ip,objv[3],&(e->gen))) 
        FREEEANDRETERR;
    for (i=0;i<objc2;i++) {
        if (TCL_OK != Tcl_GetIntFromObj(ip,objv2[i],&aux)) 
            FREEEANDRETERR;
        if (aux<0) pad = -1;
        e->dat[i] = aux;
    }
    for (;i<NALG;) e->dat[i++] = pad;
    PTR1(objPtr) = e;
    objPtr->typePtr = &tclExmo;
    return TCL_OK;
}

/* Create a new list Obj from the objPtr */
Tcl_Obj *Tcl_NewListFromExmo(Tcl_Obj *objPtr) {
    exmo *e = (exmo *) PTR1(objPtr);
    Tcl_Obj *res, *(objv[4]), **arr; 
    int i, len = exmoGetLen(e);
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

void copyStringRep(Tcl_Obj *dest, Tcl_Obj *src) {
    int slen; char *str = Tcl_GetStringFromObj(src, &slen);
    dest->bytes = ckalloc(slen + 1);
    memcpy(dest->bytes, str, slen + 1);
    dest->length = slen;
}

/* recreate string representation */
void ExmoUpdateStringProc(Tcl_Obj *objPtr) {
    copyStringRep(objPtr, Tcl_NewListFromExmo(objPtr));
}

/* create copy */
void ExmoDupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    exmo *new = (exmo *) malloc(sizeof(exmo));
    TCLMEMASSERT(new); 
    memcpy(new, PTR1(srcPtr), sizeof(exmo));
    PTR1(dupPtr) = new;
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

int Tcl_ObjIsPoly(Tcl_Obj *obj) { return &tclPoly == obj->typePtr; }

polyType *polyTypeFromTclObj(Tcl_Obj *obj) { return (polyType *) PTR1(obj); }
void     *polyFromTclObj(Tcl_Obj *obj)     { return PTR2(obj); }

Tcl_Obj *Tcl_NewPolyObj(polyType *tp, void *data) {
    Tcl_Obj *res = Tcl_NewObj();
    PTR1(res) = (void *) tp;
    PTR2(res) = data;
    res->typePtr = &tclPoly;
    return res;
}

Tcl_Obj *Tcl_NewListFromPoly(Tcl_Obj *obj) {
    int i, len = PLgetLength((polyType *) PTR1(obj),PTR2(obj));
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
    PLfree((polyType *) PTR1(obj),PTR2(obj));
}

/* try to turn objPtr into a Poly */
int PolySetFromAnyProc(Tcl_Interp *ip, Tcl_Obj *objPtr) {
    int objc, i;
    void *pol;
    Tcl_Obj **objv;
    if (TCL_OK != Tcl_ListObjGetElements(ip, objPtr, &objc, &objv))
        return TCL_ERROR;
    for (i=0;i<objc;i++)
        if (TCL_OK != Tcl_ConvertToExmo(ip,objv[i]))
            return TCL_ERROR;
    /* now we are a list of exmo objects */
    if (NULL == (pol = (stdpoly->createCopy)(NULL))) 
        RETERR("out of memory");
    for (i=0;i<objc;i++) 
        if (SUCCESS != PLappendExmo(stdpoly,pol,exmoFromTclObj(objv[i]))) {
            (stdpoly->free)(pol);
            RETERR("out of memory");
        }
    (objPtr->typePtr->freeIntRepProc)(objPtr);
    PTR1(objPtr) = stdpoly;
    PTR2(objPtr) = pol;
    objPtr->typePtr = &tclPoly;
    return TCL_OK;
}

/* recreate string representation */
void PolyUpdateStringProc(Tcl_Obj *objPtr) {
    copyStringRep(objPtr, Tcl_NewListFromPoly(objPtr));
}

/* create copy */
void PolyDupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    polyType *stp = (polyType *) PTR1(srcPtr);
    PTR1(dupPtr) = (void *) stp;
    PTR2(dupPtr) = (stp->createCopy)(PTR2(srcPtr));
    dupPtr->typePtr = &tclPoly;
}

typedef enum { 
    TPEXMO, TPPOLY, 
    TPCANCEL
} PolyCmdCode;

int tPolyCombiCmd(ClientData cd, Tcl_Interp *ip, 
                  int objc, Tcl_Obj * CONST objv[]) {
    PolyCmdCode cdi = (PolyCmdCode) cd;
    int ivar;

    switch (cdi) {
        case TPEXMO:
            ENSUREARGS1(TP_EXMO);
            Tcl_InvalidateStringRep(objv[1]);
            Tcl_SetObjResult(ip, objv[1]);
            return TCL_OK;
        case TPPOLY:
            ENSUREARGS1(TP_PLO);
            Tcl_InvalidateStringRep(objv[1]);
            Tcl_SetObjResult(ip, objv[1]);
            return TCL_OK;
        case TPCANCEL:
            ENSUREARGS3(TP_PLO,TP_OPTIONAL,TP_INT);
            if (3 == objc) Tcl_GetIntFromObj(ip, objv[2], &ivar);
            else ivar = 0;
            PLcancel(PTR1(objv[1]),PTR2(objv[1]),ivar);
            Tcl_InvalidateStringRep(objv[1]);
            Tcl_SetObjResult(ip, objv[1]);
            return TCL_OK;
    }

    Tcl_SetResult(ip, "tPolyCombiCmd: internal error", TCL_STATIC);
    return TCL_ERROR;
}

#define CREATECMD(name, id) \
  Tcl_CreateObjCommand(ip, "tpoly::" name, tPolyCombiCmd, \
                       (ClientData) id, NULL)

int Tpoly_Init(Tcl_Interp *ip) {
    
    Tcl_InitStubs(ip, "8.0", 0);

    Tptr_Init(ip);
    Tprime_Init(ip);

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

    CREATECMD("exmocheck",   TPEXMO);
    CREATECMD("polycheck",   TPPOLY);

    CREATECMD("cancel",TPCANCEL);

    return TCL_OK;
}
