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

#include "tenum.h"
#include "enum.h"

#define RETERR(errmsg) \
{ if (NULL != ip) Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; }

/* A tclEnumerator is an enumerator plus configuration objects.
 * The configuration flags indicate whether the option has been changed. */

typedef struct {
    /* Configuration values & flags. */
    Tcl_Obj *prime, *alg, *pro, *ideg, *edeg, *hdeg, *genlist;
    int     cprime, calg, cpro, cideg, cedeg, chdeg, cgenlist;

    /* The actual data. */
    enumerator *enm;
} tclEnum;

#define TRYFREEOBJ(obj) { if (NULL != (obj)) Tcl_DecrRefCount(obj); }

int Tcl_EnumBasisCmd(ClientData cd, Tcl_Interp *ip, 
                      int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd;
    
    return TCL_OK;
} 

int Tcl_EnumSeqnoCmd(ClientData cd, Tcl_Interp *ip, 
                      int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd;
    
    return TCL_OK;
} 

int Tcl_EnumConfigureCmd(ClientData cd, Tcl_Interp *ip, 
                      int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd;
    
    return TCL_OK;
} 

int Tcl_EnumCgetCmd(ClientData cd, Tcl_Interp *ip, 
                    int objc, Tcl_Obj * const objv[]) {
    tclEnum *te = (tclEnum *) cd;
    
    return TCL_OK;
} 

typedef enum { CGET, CONFIGURE, BASIS, SEQNO } enumcmdcode;

int Tcl_EnumWidgetCmd(ClientData cd, Tcl_Interp *ip, 
                      int objc, Tcl_Obj * const objv[]) {

    static CONST char *cmdNames[] = { "cget", "configure", 
                                      "basis", "seqno", 
                                      (char *) NULL };
    static enumcmdcode cmdmap[] = { CGET, CONFIGURE, BASIS, SEQNO };

    int result;
    int index;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "option ?args?");
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(ip, objv[1], cmdNames, "option", 0, &index);
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
    
    if (NULL == (te = mallox(sizeof(tclEnum))))
        RETERR("out of memory");

    if (NULL == (te->enm = enmCreate())) 
        RETERR("out of memory");

    /* set empty (= default) options */
    te->prime = te->alg = te->pro = te->ideg = te->edeg = te->hdeg 
        = te->genlist = NULL;

    /* mark all options as changed */
    te->cprime = te->calg = te->cpro = te->cideg = te->cedeg = te->chdeg 
        = te->cgenlist = 1;
    
    if (TCL_OK != Tcl_EnumConfigureCmd((ClientData) te, ip, objc-2, objv+2)) {
        Tcl_DestroyEnum((ClientData) te);
        return TCL_ERROR;
    }

    Tcl_CreateObjCommand(ip, Tcl_GetString(objv[1]), 
                     Tcl_EnumWidgetCmd, (ClientData) te, Tcl_DestroyEnum);

    return TCL_OK;
}

int Tenum_Init(Tcl_Interp *ip) {
    
    Tcl_CreateObjCommand(ip, "epol::enumerator", 
                         Tcl_CreateEnumCmd, (ClientData) 0, NULL);

    return TCL_OK;
}
