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

/* The Tcl type for extended monomials. We use PTR1 as a pointer to 
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

/* free internal representation */
void ExmoFreeInternalRepProc(Tcl_Obj *obj) {
    ckfree(PTR1(obj));
}

#define FREEEANDRETERR { ckfree((char *) e); return TCL_ERROR; }

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
    e = (exmo *) ckalloc(sizeof(exmo));
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
Tcl_Obj *ExmoToListObj(Tcl_Obj *objPtr) {
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
    copyStringRep(objPtr, ExmoToListObj(objPtr));
}

/* create copy */
void ExmoDupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    exmo *new = (exmo *) ckalloc(sizeof(exmo));
    memcpy(new, PTR1(srcPtr), sizeof(exmo));
    PTR1(dupPtr) = new;
}

/* old stuff follows */

int appendMonoFromList(Tcl_Interp *ip, poly *p, Tcl_Obj *obj) {
    Tcl_Obj **objv;
    int objc;
    int pos=1, ivar, i, privateInt; 
    mono m;
    /* printf("appendMonoFromList %s\n",Tcl_GetString(obj)); */
    if (TCL_OK != Tcl_ListObjGetElements(ip, obj, &objc, &objv)) 
        return TCL_ERROR;
    if (objc < 3)  
        RETERR("wrong format! should be: coeff, ext, id, dat0, dat1,...");
    clearMono(&m);
    GETINT(objv[0], (m.coeff));
    GETINT(objv[1], (m.ext));
    GETINT(objv[2], (m.id));
    for (i=0;3+i<objc;i++) {
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3+i], &ivar)) 
            return TCL_ERROR;
        if (i>=NALG) RETERR("NALG too small") ;
        m.dat[i] = ivar;
    }
    pos = (ivar >= 0) ;
    for (;i<NALG;i++) m.dat[i] = pos ? 0 : -1 ;
    appendMono(p, &m);
    return TCL_OK;
}

/* append data from obj to p */
int polyFromList(Tcl_Interp *ip, poly *p, Tcl_Obj *obj) {
    Tcl_Obj **objv;
    int objc;
    if (TCL_OK != Tcl_ListObjGetElements(ip, obj, &objc, &objv)) 
        return TCL_ERROR;
    for (; objc; objc--, objv++)
        if (TCL_OK != appendMonoFromList(ip, p, *objv)) 
            return TCL_ERROR;
    return TCL_OK;
}

/* return poly as list of [list coeff ext id [dat0 ... dat1]] */
int polyToList(Tcl_Interp *ip, poly *p) {
    int i,j; 
    Tcl_Obj **arr;
    arr = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * p->num);
    for (i=0;i<p->num;i++) {
        Tcl_Obj **aux = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * (3 + NALG));
        mono *m;
        m = p->dat + i;
        aux[0] = Tcl_NewIntObj(m->coeff);
        aux[1] = Tcl_NewIntObj(m->ext);
        aux[2] = Tcl_NewIntObj(m->id);
        for (j=0;j<NALG;j++) 
            aux[3+j] = Tcl_NewIntObj(m->dat[j]);
        arr[i] = Tcl_NewListObj(3+NALG, aux);
        ckfree((char *) aux);
    }
    Tcl_SetObjResult(ip, Tcl_NewListObj(p->num, arr));
    ckfree((char *) arr);
    return TCL_OK;
}

typedef enum { TPEXMO,
    TPCREATE, TPDISPOSE, TPGETNUM, TPGETALLOC, TPGETMCOFF, TPSETMCOFF, 
    TPGETDATA, TPSETDATA, TPSORT, TPCOMPACT, TPREFLECT, TPPOLPOS, 
    TPPOLNEG, TPCLEAR, TPGETNALG, TPCOPY, TPAPPENDDATA, TPSTMULT, 
    TPADDSCALED, TPSHIFTENTRY, TPRAISEPPOW 
} PolyCmdCode;

int tPolyCombiCmd(ClientData cd, Tcl_Interp *ip, 
                  int objc, Tcl_Obj * CONST objv[]) {
    PolyCmdCode cdi = (PolyCmdCode) cd;
    poly *res, *res2, *res3, *res4;
    int privateInt, ivar, ivar2, ivar3;
    primeInfo *pi;

    switch (cdi) {
        case TPCREATE: 
            ENSUREARGS0;
            if (NULL == (res = createPoly(5))) RETERR("out of memory");
            Tcl_SetObjResult(ip, Tcl_NewTPtr(TP_POLY, res));
            return TCL_OK;
        case TPDISPOSE:
            ENSUREARGS1(TP_POLY);
            res = (poly *) TPtr_GetPtr(objv[1]);
            disposePoly(res); 
            return TCL_OK;
        case TPCLEAR: 
            ENSUREARGS1(TP_POLY);
            res = (poly *) TPtr_GetPtr(objv[1]);
            clearPoly(res); return TCL_OK;
        case TPGETNUM:   
            ENSUREARGS1(TP_POLY);
            res = (poly *) TPtr_GetPtr(objv[1]);
            RETINT(res->num); 
        case TPGETNALG:  
            ENSUREARGS1(TP_POLY);
            res = (poly *) TPtr_GetPtr(objv[1]);
            RETINT(NALG);
        case TPGETALLOC: 
            ENSUREARGS1(TP_POLY);
            res = (poly *) TPtr_GetPtr(objv[1]);
            RETINT(res->numAlloc);
        case TPGETMCOFF: 
            ENSUREARGS1(TP_POLY);
            res = (poly *) TPtr_GetPtr(objv[1]);
            RETINT(res->maxcoeff);
        case TPSETMCOFF: 
            ENSUREARGS2(TP_POLY,TP_INT);
            res = (poly *) TPtr_GetPtr(objv[1]);
            Tcl_GetIntFromObj(ip, objv[2], &ivar);
            res->maxcoeff = ivar;
            return TCL_OK;
        case TPSORT:
            ENSUREARGS1(TP_POLY);
            res = (poly *) TPtr_GetPtr(objv[1]);
            sortPoly(res); return TCL_OK;
        case TPCOMPACT:
            ENSUREARGS1(TP_POLY);
            res = (poly *) TPtr_GetPtr(objv[1]);
            compactPoly(res); return TCL_OK;
        case TPREFLECT:
            ENSUREARGS2(TP_POLY,TP_POLY);
            res  = (poly *) TPtr_GetPtr(objv[1]);
            res2 = (poly *) TPtr_GetPtr(objv[2]);
            polyCompCopy(res, res2, monoReflect);
            return TCL_OK;
        case TPCOPY:
            ENSUREARGS2(TP_POLY,TP_POLY);
            res  = (poly *) TPtr_GetPtr(objv[1]);
            res2 = (poly *) TPtr_GetPtr(objv[2]);
            polyCompCopy(res, res2, copyMono);
            return TCL_OK;
        case TPPOLPOS:
            ENSUREARGS3(TP_POLY,TP_POLY,TP_POLY);
            res  = (poly *) TPtr_GetPtr(objv[1]);
            res2 = (poly *) TPtr_GetPtr(objv[2]);
            res3 = (poly *) TPtr_GetPtr(objv[3]);
            clearPoly(res);
            polyAppendMult(res, res2, res3, multMonoPosPoly);
            return TCL_OK;
        case TPPOLNEG:
            ENSUREARGS3(TP_POLY,TP_POLY,TP_POLY);
            res  = (poly *) TPtr_GetPtr(objv[1]);
            res2 = (poly *) TPtr_GetPtr(objv[2]);
            res3 = (poly *) TPtr_GetPtr(objv[3]);
            clearPoly(res);
            polyAppendMult(res, res2, res3, multMonoNegPoly);
            return TCL_OK;
        case TPGETDATA:
            ENSUREARGS1(TP_POLY);
            res = (poly *) TPtr_GetPtr(objv[1]);
            return polyToList(ip, res);
        case TPSETDATA:
            ENSUREARGS2(TP_POLY,TP_ANY);
            res = (poly *) TPtr_GetPtr(objv[1]);
            clearPoly(res); 
            /* fall through */
        case TPAPPENDDATA:
            ENSUREARGS2(TP_POLY,TP_ANY);
            res = (poly *) TPtr_GetPtr(objv[1]);
            return polyFromList(ip, res, objv[2]);
        case TPSTMULT:        
            ENSUREARGS4(TP_PRIME,TP_POLY,TP_POLY,TP_POLY);
            if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[1], &pi)) return TCL_ERROR;
            res2 = (poly *) TPtr_GetPtr(objv[2]);
            res3 = (poly *) TPtr_GetPtr(objv[3]);
            res4 = (poly *) TPtr_GetPtr(objv[4]);
            multPoly(pi, res2, res3, res4, multCBaddToPoly);
            return TCL_OK;
        case TPADDSCALED:
            ENSUREARGS3(TP_POLY,TP_POLY,TP_INT);
            res  = (poly *) TPtr_GetPtr(objv[1]);
            res2 = (poly *) TPtr_GetPtr(objv[2]);
            Tcl_GetIntFromObj(ip, objv[3], &ivar2);
            appendScaledPoly(res, res2, ivar2);  
            return TCL_OK;
        case TPSHIFTENTRY:
            ENSUREARGS3(TP_POLY,TP_INT,TP_INT);
            res  = (poly *) TPtr_GetPtr(objv[1]);
            Tcl_GetIntFromObj(ip, objv[2], &ivar2);
            Tcl_GetIntFromObj(ip, objv[3], &ivar3);
            polyShiftEntry(res, ivar2, ivar3);  
            return TCL_OK;
        case TPRAISEPPOW:
            ENSUREARGS2(TP_POLY,TP_INT);
            res = (poly *) TPtr_GetPtr(objv[1]);
            Tcl_GetIntFromObj(ip, objv[2], &ivar2);
            GETINT(objv[2], ivar2); 
            polyRaisePPow(res, ivar2);  
            return TCL_OK;
        case TPEXMO:
            ENSUREARGS1(TP_EXMO);
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

    TPtr_RegType(TP_POLY, "poly");

    CREATECMD("exmo",TPEXMO);

    CREATECMD("create",   TPCREATE);
    CREATECMD("dispose",  TPDISPOSE);
    CREATECMD("getNum",   TPGETNUM);
    CREATECMD("getAlloc", TPGETALLOC);
    CREATECMD("getMaxcoeff", TPGETMCOFF);
    CREATECMD("setMaxcoeff", TPSETMCOFF);
    CREATECMD("getData",    TPGETDATA);
    CREATECMD("setData",    TPSETDATA);
    CREATECMD("appendData", TPAPPENDDATA);
    CREATECMD("sort",    TPSORT);
    CREATECMD("compact", TPCOMPACT);
    CREATECMD("reflect", TPREFLECT);
    CREATECMD("multPolPos", TPPOLPOS);
    CREATECMD("multPolNeg", TPPOLNEG);
    CREATECMD("clear", TPCLEAR);
    CREATECMD("getNALG", TPGETNALG);
    CREATECMD("copy", TPCOPY);
    CREATECMD("stmult", TPSTMULT);
    CREATECMD("addScaled", TPADDSCALED);
    CREATECMD("shiftEntry", TPSHIFTENTRY);
    CREATECMD("raisePPow", TPRAISEPPOW);

    return TCL_OK;
}
