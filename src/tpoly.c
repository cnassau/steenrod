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

#define RETERR(errmsg) \
{ Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; } 

#define RETINT(i) { Tcl_SetObjResult(ip, Tcl_NewIntObj(i)); return TCL_OK; }

#define GETINT(ob, var)                                \
 if (TCL_OK != Tcl_GetIntFromObj(ip, ob, &privateInt)) \
    return TCL_ERROR;                                  \
 var = privateInt;

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

typedef enum {
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
        ENSUREARGS4(TP_PRINFO,TP_POLY,TP_POLY,TP_POLY);
        pi   = (primeInfo *) TPtr_GetPtr(objv[1]);
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

    TPtr_RegType(TP_POLY, "poly");
    
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
