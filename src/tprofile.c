/*
 * Tcl interface to the profiles, algebras and enumeration stuff
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
#include "tprime.h"
#include "tprofile.h"

typedef enum {
    CORE_SET, CORE_GETEXT, CORE_GETRED,
    PR_CREATE, PR_DESTROY, PR_SET, PR_GET, ENV_CREATE, ENV_DISPOSE, 
    EXM_CREATE, EXM_DISPOSE, EXM_FIRST, EXM_NEXT
} ProfileCmdCode;

int tProfileCombiCmd(ClientData cd, Tcl_Interp *ip, 
                     int objc, Tcl_Obj *CONST(objv[])) {

    ProfileCmdCode cdi = (ProfileCmdCode) cd;
    profile *prof, *alg;
    enumEnv *env;
    exmon *exmo; 
    primeInfo *pi;
    procore *core;
    int i1,i2,i3;

    switch (cdi) {
        case CORE_SET:
            ENSUREARGS3(TP_PROCORE, TP_INT, TP_LIST);
            core = (procore *) TPtr_GetPtr(objv[1]);
            clearProcore(core);
            Tcl_GetIntFromObj(ip, objv[2], &i1);
            core->edat=i1;
            
            return TCL_OK;
        case CORE_GETEXT:
            ENSUREARGS1(TP_PROCORE);
            core = (procore *) TPtr_GetPtr(objv[1]); 
            Tcl_SetObjResult(ip, Tcl_NewIntObj(core->edat));
            return TCL_OK;
        case CORE_GETRED:
            ENSUREARGS1(TP_PROCORE);
            core = (procore *) TPtr_GetPtr(objv[1]); 
            Tcl_SetObjResult(ip, Tcl_ListFromArray(NPRO, core->rdat));
            return TCL_OK;
        case PR_CREATE:
            return TCL_OK;
        case PR_DESTROY: 
            return TCL_OK;
        case PR_SET: 
            return TCL_OK;
        case PR_GET: 
            
            return TCL_OK;
        case ENV_CREATE: 
            return TCL_OK;
        case ENV_DISPOSE: 
            return TCL_OK;
        case EXM_CREATE: 
            return TCL_OK;
        case EXM_DISPOSE: 
            return TCL_OK;
        case EXM_FIRST: 
            return TCL_OK;
        case EXM_NEXT:
        default:    
            Tcl_SetResult(ip, 
                          "error in tProfileCombiCmd: command not implemented", 
                          TCL_STATIC);
            return TCL_ERROR;        
    }

    /* not reached */

    Tcl_SetResult(ip, "internal error in tProfileCombiCmd", TCL_STATIC);
    return TCL_ERROR;
}


#define NSP "profile::"

int Tprofile_IsInitialized;

int Tprofile_Init(Tcl_Interp *ip) {

    if (Tprofile_IsInitialized) return TCL_OK;
    Tprofile_IsInitialized = 1;
    
    if (NULL == Tcl_InitStubs(ip, "8.0", 0)) return TCL_ERROR;
    
    Tptr_Init(ip);
    Tprime_Init(ip);

    TPtr_RegType(TP_PROCORE, "procore");
    TPtr_RegType(TP_PROFILE, "profile");
    TPtr_RegType(TP_EXMON,   "extended monomial");
    TPtr_RegType(TP_ENVMNT,  "environment");

#define CREATECOMMAND(name, code) \
Tcl_CreateObjCommand(ip,NSP name,tProfileCombiCmd,(ClientData) code, NULL);

    CREATECOMMAND("create",   PR_CREATE);

    return TCL_OK;
}
