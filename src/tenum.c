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

#include <string.h>
#include "tprime.h"
#include "tpoly.h"
#include "poly.h"
#include "tenum.h"
#include "enum.h"

#define RETERR(errmsg) \
{ if (NULL != ip) Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; }

/* A tclEnumerator is an enumerator plus configuration objects.
 * The configuration flags indicate whether the option has been changed. */

typedef struct {
    /* Configuration values & flags. */
    Tcl_Obj *prime, *alg, *pro, *sig, *ideg, *edeg, *hdeg, *genlist;
    int     cprime, calg, cpro, csig, cideg, cedeg, chdeg, cgenlist;
    
    /* new genlist */
    int *gl, gllength;

    /* The actual data. */
    enumerator *enm;
} tclEnum;

#define TRYFREEOBJ(obj) { if (NULL != (obj)) Tcl_DecrRefCount(obj); }

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

    if (NULL == (res = (int *) callox(objc * 4, sizeof(int))))
        return NULL;
    
    for (wrk=res; objc--; (objv)++,wrk+=4) {
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

/* Here we try to activate the configured values. */

int Tcl_EnumSetValues(ClientData cd, Tcl_Interp *ip) {
    tclEnum *te = (tclEnum *) cd;
    primeInfo *pi;
    exmo *alg, *pro, *sig;

#define TRYGETEXMO(ex,obj)                          \
if (NULL != (obj)) {                                \
   if (TCL_OK != Tcl_ConvertToExmo(ip, (obj)))      \
       return TCL_ERROR;                            \
   ex = exmoFromTclObj(obj);                        \
} else ex = (exmo *) NULL; 

    if (te->cprime || te->calg || te->cpro) {
        if (NULL == te->prime) RETERR("prime not given");
        if (TCL_OK != Tcl_GetPrimeInfo(ip, te->prime, &pi))
            return TCL_ERROR;

        TRYGETEXMO(alg,te->alg);
        TRYGETEXMO(pro,te->pro);
        
        enmSetBasics(te->enm, pi, alg, pro);
        te->cprime = te->calg = te->cpro = 0;
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
        te->gl = NULL;
        te->cgenlist = 0;
    }

    return TCL_OK;
}

typedef enum  { PRIME, ALGEBRA, PROFILE, SIGNATURE, 
                IDEG, EDEG, HDEG, GENLIST } enumoptcode;

static CONST char *optNames[] = { "-prime", "-algebra", "-profile", "-signature",
                                  "-ideg", "-edeg", "-hdeg", "-genlist", 
                                  (char *) NULL };

static enumoptcode optmap[] = { PRIME, ALGEBRA, PROFILE, SIGNATURE,
                                IDEG, EDEG, HDEG, GENLIST };

/* note that this command differs from the others: it assumes that option value 
 * pairs start at objv[0]. */
int Tcl_EnumConfigureCmd(ClientData cd, Tcl_Interp *ip, 
                      int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd, aux;
 
    int result;
    int index;

    primeInfo *pi; 

    if (0 == objc) {
        Tcl_Obj *(co[2]), *res = Tcl_NewListObj(0, NULL);

#define APPENDOPT(name, obj) {                                          \
co[0] = Tcl_NewStringObj(name,strlen(name));                            \
co[1] = (NULL != (obj)) ? (obj) : Tcl_NewObj();                         \
if (TCL_OK != Tcl_ListObjAppendElement(ip, res, Tcl_NewListObj(2,co)))  \
   { Tcl_DecrRefCount(res); return TCL_ERROR; };                        \
}

        /* return configuration */
        APPENDOPT("-prime", te->prime);
        APPENDOPT("-algebra", te->alg);
        APPENDOPT("-profile", te->pro);
        APPENDOPT("-signature", te->sig);
        APPENDOPT("-ideg", te->ideg);
        APPENDOPT("-edeg", te->edeg);
        APPENDOPT("-hdeg", te->hdeg);
        /* TODO: recreate genlist from enum if that has been changed by addgen cmd */
        APPENDOPT("-genlist", te->genlist);
        Tcl_SetObjResult(ip, res);
        return TCL_OK;
    }

    /* aux keeps the new config values, before they have been validified */
    memset(&aux, 0, sizeof(tclEnum));

    if (0 != (objc & 1)) RETERR("option/value pairs excpected");

    for (; objc; objc-=2,objv+=2) {
        result = Tcl_GetIndexFromObj(ip, objv[0], optNames, "option", 0, &index);
        if (TCL_OK != result) return result;
        switch (optmap[index]) {
            case PRIME:
                if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[1], &pi)) 
                    return TCL_ERROR;
                aux.prime = objv[1];
                break;
            case ALGEBRA:
                if (TCL_OK != Tcl_ConvertToExmo(ip, objv[1])) return TCL_ERROR;
                aux.alg = objv[1];
                break;
            case PROFILE:
                if (TCL_OK != Tcl_ConvertToExmo(ip, objv[1])) return TCL_ERROR;
                aux.pro = objv[1];
                break;
            case SIGNATURE:
                if (TCL_OK != Tcl_ConvertToExmo(ip, objv[1])) return TCL_ERROR;
                aux.sig = objv[1];
                break;
            case IDEG:
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[1], &(aux.cideg))) 
                    return TCL_ERROR;
                aux.ideg = objv[1]; 
                break;
            case EDEG:
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[1], &(aux.cedeg))) 
                    return TCL_ERROR;
                aux.edeg = objv[1];
                break;
            case HDEG:
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[1], &(aux.chdeg))) 
                    return TCL_ERROR;
                aux.hdeg = objv[1]; 
                break;
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
        primeInfo *pi2 = NULL;
        if (NULL != te->prime) Tcl_GetPrimeInfo(ip, te->prime, &pi2); 
        if ((NULL == te->prime) || (pi2 != pi)) {
            TRYFREEOBJ(te->prime); 
            te->prime = aux.prime; 
            Tcl_IncrRefCount(te->prime);
            te->cprime = 1;
        }
    }

#define SETOPT(old,new,flag) \
if (NULL != new) { TRYFREEOBJ(old); old = new; Tcl_IncrRefCount(old); flag = 1; }

    SETOPT(te->alg, aux.alg, te->calg);
    SETOPT(te->pro, aux.pro, te->cpro);
    SETOPT(te->sig, aux.sig, te->csig);
    SETOPT(te->ideg, aux.ideg, te->cideg);
    SETOPT(te->edeg, aux.edeg, te->cedeg);    
    SETOPT(te->hdeg, aux.hdeg, te->chdeg);       
  
    if (NULL != aux.genlist) {
        TRYFREEOBJ(te->genlist); 
        te->genlist = aux.genlist; 
        aux.genlist = NULL; te->cgenlist = 1;
        Tcl_IncrRefCount(te->genlist);
    }

    return TCL_OK;
} 

int Tcl_EnumCgetCmd(ClientData cd, Tcl_Interp *ip, 
                    int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd;
    int result, index;

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
        case SIGNATURE: SETRESRET(te->sig);
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
            Tcl_Obj *aux =  Tcl_NewExmoCopyObj(&(te->enm->varex));
            Tcl_AppendElement(ip, Tcl_GetString(aux));
            Tcl_DecrRefCount(aux);
        } while (nextRedmon(te->enm));
    
    return TCL_OK;
} 

int Tcl_EnumSeqnoCmd(ClientData cd, Tcl_Interp *ip, 
                      int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd;
 
    if (TCL_OK != Tcl_EnumSetValues(cd, ip)) return TCL_ERROR;
   
    return TCL_OK;
} 

typedef enum { CGET, CONFIGURE, BASIS, SEQNO } enumcmdcode;

static CONST char *cmdNames[] = { "cget", "configure", 
                                  "basis", "seqno", 
                                  (char *) NULL };

static enumcmdcode cmdmap[] = { CGET, CONFIGURE, BASIS, SEQNO };

int Tcl_EnumWidgetCmd(ClientData cd, Tcl_Interp *ip, 
                      int objc, Tcl_Obj * const objv[]) {

    int result;
    int index;

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

    Tcl_CreateObjCommand(ip, "epol::enumerator", 
                         Tcl_CreateEnumCmd, (ClientData) 0, NULL);

    return TCL_OK;
}
