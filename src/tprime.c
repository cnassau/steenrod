/*
 * Tcl interface to the basic prime stuff
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
#include "tptr.h"
#include "prime.h"

/* client data to distinguish the subcommands */
#define CD_CREATE      1
#define CD_DISPOSE     2
#define CD_MAXDEG      3
#define CD_TPMO        4
#define CD_N           5
#define CD_PRIMPOWS    6
#define CD_REDDEGS     7
#define CD_EXTDEGS     8
#define CD_INVERSE     9
#define CD_BINOM       10
#define CD_PRIME       11
    
#define RETURNINT(rval) Tcl_SetObjResult(ip,Tcl_NewIntObj(rval)); return TCL_OK
#define RETURNLIST(list,len) \
Tcl_SetObjResult(ip,Tcl_ListFromArray(len,list)); return TCL_OK

int tPrInfo(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[]) {
    int cdi = (int) cd;
    int a, b;
    primeInfo *pi;

    /* the initial switch just implements create & dispose;
     * for other commands we only check the arguments here */
    switch (cdi) {
        case CD_INVERSE:
            if (TCL_OK!=TPtr_CheckArgs(ip,objc,objv,TP_PRIME,TP_INT,TP_END))
                return TCL_ERROR;
            Tcl_GetIntFromObj(ip, objv[2], &a);
            break;
        case CD_BINOM:
            if (TCL_OK!=TPtr_CheckArgs(ip,objc,objv,
                                       TP_PRIME,TP_INT,TP_INT,TP_END))
                return TCL_ERROR;
            Tcl_GetIntFromObj(ip, objv[2], &a);
            Tcl_GetIntFromObj(ip, objv[3], &b);
            break;
        default:
            if (TCL_OK != TPtr_CheckArgs(ip, objc, objv, TP_PRIME, TP_END))
                return TCL_ERROR;
    }
    
    if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[1], &pi)) return TCL_ERROR;

    switch (cdi) {
        case CD_BINOM: RETURNINT(binomp(pi, a, b)) ;
        case CD_INVERSE:
            a %= pi->prime;
            if (0 == a) {
                Tcl_SetResult(ip, "division by zero", TCL_VOLATILE);
                return TCL_ERROR;
            }
            RETURNINT(pi->inverse[a]);
        case CD_PRIME: RETURNINT(pi->prime);
        case CD_MAXDEG: RETURNINT(pi->maxdeg);
        case CD_N: RETURNINT(NALG);
        case CD_TPMO: RETURNINT(pi->tpmo);
        case CD_REDDEGS: RETURNLIST(pi->reddegs, NALG);
        case CD_EXTDEGS: RETURNLIST(pi->extdegs, NALG);
        case CD_PRIMPOWS: RETURNLIST(pi->primpows, NALG);
    } 
    
    Tcl_SetResult(ip, "internal error in tPtrInfo", TCL_VOLATILE);
    return TCL_ERROR;
}

/* List of primeInfo structures that have been constructed */
typedef struct piList {
    primeInfo pi;
    struct piList *next;
} piList; 

static piList *piMasterList;

/* return values come from makePrimeInfo */
int findPrimeInfo(int prime, primeInfo **pi) {
    piList **nextp = &(piMasterList);
    int rcode;
    for (; NULL != *nextp; nextp = &((*nextp)->next)) 
        if (prime == (*nextp)->pi.prime) { *pi = &((*nextp)->pi); return PI_OK; }
    if (NULL == ((*nextp) = malloc(sizeof(piList)))) return PI_NOMEM;
    if (PI_OK != (rcode = makePrimeInfo(&((*nextp)->pi), prime))) { 
        free(*nextp); 
        *nextp = NULL; 
        return rcode; 
    }
    (*nextp)->next = NULL;
    *pi = &((*nextp)->pi);
    return PI_OK;
}

static Tcl_ObjType PrimeType;

/* try to turn objPtr into a PrimeType object */
int PT_SetFromAnyProc(Tcl_Interp *interp, Tcl_Obj *objPtr) {
    int rc, prime;
    primeInfo *pi;
    if (objPtr->typePtr == &PrimeType) 
        return TCL_OK;
    if (TCL_OK != Tcl_GetIntFromObj(interp, objPtr, &prime)) 
        return TCL_ERROR;
    if (PI_OK != (rc = findPrimeInfo(prime, &pi))) {
        const char *err;
        switch (rc) {
            case PI_NOPRIME:   err = "not a prime number"; break;
            case PI_TOOLARGE:  err = "number too large";   break;
            case PI_NOMEM:     err = "out of memory (makePrimeInfo)"; break;
            case PI_STRANGE:   err = "strange error (makePrimeInfo)"; break;
            default:  err = "unknown error (makePrimeInfo)";
        }
        if (NULL != interp) Tcl_SetResult(interp, (char *) err, TCL_STATIC);
        return TCL_ERROR;
    } 
    objPtr->typePtr = &PrimeType;
    objPtr->internalRep.twoPtrValue.ptr1 = (void *) pi;
    return TCL_OK;
}

/* recreate string representation */
void PT_UpdateStringProc(Tcl_Obj *objPtr) {
    primeInfo *pi = (primeInfo *) objPtr->internalRep.twoPtrValue.ptr1;
    objPtr->bytes = ckalloc(5);
    sprintf(objPtr->bytes, "%d", pi->prime);
    objPtr->length = strlen(objPtr->bytes);
}

/* create copy */
void PT_DupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    dupPtr->typePtr = &PrimeType;
    dupPtr->internalRep.twoPtrValue.ptr1 = srcPtr->internalRep.twoPtrValue.ptr1;
}

int Tcl_GetPrimeInfo(Tcl_Interp *ip, Tcl_Obj *obj, primeInfo **pi) {
    if (TCL_OK != Tcl_ConvertToType(ip, obj, &PrimeType)) return TCL_ERROR;
    *pi = (primeInfo *) obj->internalRep.twoPtrValue.ptr1;
    return TCL_OK;
}

/* our namespace */
#define NSP "primestuff::"

int Tprime_HaveType; 

int Tprime_Init(Tcl_Interp *ip) {

    if (NULL == Tcl_InitStubs(ip, "8.0", 0)) return TCL_ERROR;
    
    Tptr_Init(ip);

    if (!Tprime_HaveType) {
        /* set up type and register */
        PrimeType.name                  = "prime";
        PrimeType.freeIntRepProc        = NULL;
        PrimeType.dupIntRepProc         = PT_DupInternalRepProc;
        PrimeType.updateStringProc      = PT_UpdateStringProc;
        PrimeType.setFromAnyProc        = PT_SetFromAnyProc;
        Tcl_RegisterObjType(&PrimeType);
        
        TPtr_RegObjType(TP_PRIME, &PrimeType);
        Tprime_HaveType = 1;
    }

#define CREATECOMMAND(name, code) \
Tcl_CreateObjCommand(ip,NSP name,tPrInfo,(ClientData) code, NULL);

    CREATECOMMAND("primecheck", CD_PRIME);
    CREATECOMMAND("maxdeg",     CD_MAXDEG);
    CREATECOMMAND("tpmo",       CD_TPMO);
    CREATECOMMAND("N",          CD_N);
    CREATECOMMAND("primpows",   CD_PRIMPOWS);
    CREATECOMMAND("reddegs",    CD_REDDEGS);
    CREATECOMMAND("extdegs",    CD_EXTDEGS);
    CREATECOMMAND("inverse",    CD_INVERSE);
    CREATECOMMAND("binom",      CD_BINOM);
    
    return TCL_OK;
}
