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

#define RETERR(errmsg) \
{ Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; }

/* the following trick is from the cpp info page on stringification: */
#define xstringify(s) stringify(s)
#define stringify(s) #s

typedef enum {
    CORE_SET, CORE_GETEXT, CORE_GETRED, 
    CORE_GETREDDEG, CORE_GETEXTDEG,
    PR_CREATE, PR_DESTROY, PR_GETCORE, 
    ENV_CREATE, ENV_DISPOSE, 
    SQN_CREATE, SQN_DESTROY, SQN_GETDIM, SQN_GETSEQNO, 
    EXM_CREATE, EXM_DISPOSE, EXM_GETCORE, EXM_FIRST, EXM_NEXT, 
    EXM_FIRSTSIG, EXM_NEXTSIG,
    EXM_GETID, EXM_SETID
} ProfileCmdCode;

int tProfileCombiCmd(ClientData cd, Tcl_Interp *ip, 
                     int objc, Tcl_Obj *CONST(objv[])) {

    ProfileCmdCode cdi = (ProfileCmdCode) cd;
    profile *prof, *alg;
    enumEnv *env;
    exmon *exmo; 
    primeInfo *pi;
    procore *core;
    seqnoInfo *sqn;
    int i1,i2;

    switch (cdi) {
        case CORE_SET:
            ENSUREARGS4(TP_PROCORE, TP_INT, TP_INT, TP_INTLIST);
            core = (procore *) TPtr_GetPtr(objv[1]);
            Tcl_GetIntFromObj(ip, objv[2], &i1);
            Tcl_GetIntFromObj(ip, objv[3], &i2);
            clearProcore(core, i2);
            core->edat=i1;
            {
                int obc; Tcl_Obj **obv; 
                Tcl_ListObjGetElements(ip, objv[4], &obc, &obv);
                if (obc>=NPRO) 
                    RETERR("index too big for NPRO "
                           "(compiled maximum =" xstringify(NPRO) ")");
                for (i1=0;i1<obc;i1++) {
                    Tcl_GetIntFromObj(ip,obv[i1], &i2);
                    core->rdat[i1] = i2;
                }
            }
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
        case CORE_GETREDDEG:
            ENSUREARGS2(TP_PROCORE,TP_PRIME);
            core = (procore *) TPtr_GetPtr(objv[1]);
            if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[2], &pi)) return TCL_ERROR;
            Tcl_SetObjResult(ip, Tcl_NewIntObj(reddegProcore(core, pi)));
            return TCL_OK;
        case CORE_GETEXTDEG:
            ENSUREARGS2(TP_PROCORE,TP_PRIME);
            core = (procore *) TPtr_GetPtr(objv[1]);
            if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[2], &pi)) return TCL_ERROR;
            Tcl_SetObjResult(ip, Tcl_NewIntObj(extdeg(pi, core->edat)));
            return TCL_OK;
        case PR_CREATE:
            ENSUREARGS0;
            prof = (profile *) mallox(sizeof(profile)); 
            Tcl_SetObjResult(ip, Tcl_NewTPtr(TP_PROFILE, prof));
            return TCL_OK;
        case PR_DESTROY: 
            ENSUREARGS1(TP_PROFILE);
            prof = (profile *) TPtr_GetPtr(objv[1]); 
            freex(prof);
            return TCL_OK;
        case PR_GETCORE: 
            ENSUREARGS1(TP_PROFILE);
            prof = (profile *) TPtr_GetPtr(objv[1]); 
            core = &(prof->core);
            Tcl_SetObjResult(ip, Tcl_NewTPtr(TP_PROCORE, core));
            return TCL_OK;
        case SQN_CREATE:
            ENSUREARGS2(TP_ENENV,TP_INT);
            env = (enumEnv *) TPtr_GetPtr(objv[1]);
            Tcl_GetIntFromObj(ip, objv[2], &i1);
            sqn = createSeqno(env, i1);
            if (NULL == sqn) RETERR("Out of memory");
            Tcl_SetObjResult(ip, Tcl_NewTPtr(TP_SQINF, sqn));
            return TCL_OK;
        case SQN_DESTROY:
            ENSUREARGS1(TP_SQINF);
            sqn = (seqnoInfo *) TPtr_GetPtr(objv[1]);
            destroySeqno(sqn);
            return TCL_OK;
        case SQN_GETDIM:
            ENSUREARGS2(TP_SQINF,TP_INT);
            sqn = (seqnoInfo *) TPtr_GetPtr(objv[1]);
            Tcl_GetIntFromObj(ip, objv[2], &i1);
            Tcl_SetObjResult(ip, Tcl_NewIntObj(SqnInfGetDim(sqn, i1)));
            return TCL_OK;
        case SQN_GETSEQNO:
            ENSUREARGS3(TP_SQINF,TP_EXMON,TP_INT);
            sqn = (seqnoInfo *) TPtr_GetPtr(objv[1]);
            exmo = (exmon *) TPtr_GetPtr(objv[2]);
            Tcl_GetIntFromObj(ip, objv[3], &i1);
            Tcl_SetObjResult(ip, Tcl_NewIntObj(
                                 SqnInfGetSeqnoWithDegree(sqn, exmo, i1)));
            return TCL_OK;
        case ENV_CREATE:
            ENSUREARGS3(TP_PRIME,TP_PROFILE,TP_PROFILE);
            if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[1], &pi)) return TCL_ERROR;
            prof = (profile *) TPtr_GetPtr(objv[2]); 
            alg  = (profile *) TPtr_GetPtr(objv[3]);
            env = createEnumEnv(pi, prof, alg); 
            Tcl_SetObjResult(ip, Tcl_NewTPtr(TP_ENENV, env));
            return TCL_OK;
        case ENV_DISPOSE:
            ENSUREARGS1(TP_ENENV);
            env = (enumEnv *) TPtr_GetPtr(objv[1]); 
            disposeEnumEnv(env);
            freex(env);
            return TCL_OK;
        case EXM_CREATE:
            ENSUREARGS0;
            exmo = mallox(sizeof(exmon));
            Tcl_SetObjResult(ip, Tcl_NewTPtr(TP_EXMON, exmo));
            return TCL_OK;
        case EXM_DISPOSE:
            ENSUREARGS1(TP_EXMON); 
            exmo = (exmon *) TPtr_GetPtr(objv[1]);
            freex(exmo);
            return TCL_OK;
        case EXM_GETCORE:
            ENSUREARGS1(TP_EXMON); 
            exmo = (exmon *) TPtr_GetPtr(objv[1]);
            core = &(exmo->core);
            Tcl_SetObjResult(ip, Tcl_NewTPtr(TP_PROCORE, core));
            return TCL_OK;
        case EXM_FIRST: 
            ENSUREARGS3(TP_EXMON, TP_ENENV, TP_INT);
            exmo = (exmon *) TPtr_GetPtr(objv[1]);
            env = (enumEnv *) TPtr_GetPtr(objv[2]);
            Tcl_GetIntFromObj(ip, objv[3], &i1);
            i2 = firstExmon(exmo, env, i1);
            Tcl_SetObjResult(ip, Tcl_NewBooleanObj(i2));
            return TCL_OK;
        case EXM_NEXT:
            ENSUREARGS2(TP_EXMON, TP_ENENV);
            exmo = (exmon *) TPtr_GetPtr(objv[1]);
            env = (enumEnv *) TPtr_GetPtr(objv[2]);
            i2 = nextExmon(exmo, env);
            Tcl_SetObjResult(ip, Tcl_NewBooleanObj(i2));
            return TCL_OK;  
        case EXM_FIRSTSIG: 
            ENSUREARGS3(TP_EXMON, TP_ENENV, TP_INT);
            exmo = (exmon *) TPtr_GetPtr(objv[1]);
            env = (enumEnv *) TPtr_GetPtr(objv[2]);
            Tcl_GetIntFromObj(ip, objv[3], &i1);
            firstSignature(env, exmo, i1);
            Tcl_SetObjResult(ip, Tcl_NewBooleanObj(i2));
            return TCL_OK;
        case EXM_NEXTSIG: 
            ENSUREARGS2(TP_EXMON, TP_ENENV);
            exmo = (exmon *) TPtr_GetPtr(objv[1]);
            env = (enumEnv *) TPtr_GetPtr(objv[2]);
            i2 = nextSignature(env, exmo);
            Tcl_SetObjResult(ip, Tcl_NewBooleanObj(i2));
            return TCL_OK;
        case EXM_GETID:
            ENSUREARGS1(TP_EXMON);
            exmo = (exmon *) TPtr_GetPtr(objv[1]);
            Tcl_SetObjResult(ip, Tcl_NewIntObj(exmo->id));
            return TCL_OK;
        case EXM_SETID:
            ENSUREARGS2(TP_EXMON,TP_INT);
            exmo = (exmon *) TPtr_GetPtr(objv[1]);
            Tcl_GetIntFromObj(ip, objv[2], &i1);
            exmo->id = i1;
            return TCL_OK;
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


int Tprofile_HaveType;

int Tprofile_Init(Tcl_Interp *ip) {
    
    if (NULL == Tcl_InitStubs(ip, "8.0", 0)) return TCL_ERROR;
    
    Tptr_Init(ip);
    Tprime_Init(ip);

    if (!Tprofile_HaveType) {
        TPtr_RegType(TP_PROCORE, "procore");
        TPtr_RegType(TP_PROFILE, "profile");
        TPtr_RegType(TP_EXMON,   "extended monomial");
        TPtr_RegType(TP_ENENV,   "environment");
        Tprofile_HaveType = 1;
    }

#define CREATECOMMAND(name, code) \
Tcl_CreateObjCommand(ip,name,tProfileCombiCmd,(ClientData) code, NULL);

#define NSC "procore::"
#define NSP "profile::"
#define NSV "enumenv::"
#define NSQ "sqninfo::"
#define NSX "extmono::"

    CREATECOMMAND(NSC "set", CORE_SET); 
    CREATECOMMAND(NSC "getExt", CORE_GETEXT); 
    CREATECOMMAND(NSC "getRed", CORE_GETRED); 
    CREATECOMMAND(NSC "getReddeg", CORE_GETREDDEG); 
    CREATECOMMAND(NSC "getExtdeg", CORE_GETEXTDEG); 
    CREATECOMMAND(NSP "create", PR_CREATE); 
    CREATECOMMAND(NSP "destroy", PR_DESTROY); 
    CREATECOMMAND(NSP "getCore", PR_GETCORE); 
    CREATECOMMAND(NSV "create", ENV_CREATE); 
    CREATECOMMAND(NSV "dispose", ENV_DISPOSE); 
    CREATECOMMAND(NSQ "create", SQN_CREATE); 
    CREATECOMMAND(NSQ "destroy", SQN_DESTROY); 
    CREATECOMMAND(NSQ "getDim", SQN_GETDIM); 
    CREATECOMMAND(NSQ "getSeqno", SQN_GETSEQNO); 
    CREATECOMMAND(NSX "create", EXM_CREATE); 
    CREATECOMMAND(NSX "dispose", EXM_DISPOSE); 
    CREATECOMMAND(NSX "getCore", EXM_GETCORE); 
    CREATECOMMAND(NSX "getId", EXM_GETID); 
    CREATECOMMAND(NSX "setId", EXM_SETID); 
    CREATECOMMAND(NSX "first", EXM_FIRST); 
    CREATECOMMAND(NSX "next", EXM_NEXT);
    CREATECOMMAND(NSX "firstSig", EXM_FIRSTSIG); 
    CREATECOMMAND(NSX "nextSig", EXM_NEXTSIG);

    return TCL_OK;
}
