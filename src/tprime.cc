/*
 * Tcl interface to the basic prime stuff
 *
 * Copyright (C) 2004-2018 Christian Nassau <nassau@nullhomotopie.de>
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

#define DBGPR if (0)

#define RETURNINT(rval) { Tcl_SetObjResult(ip,Tcl_NewIntObj(rval)); return TCL_OK; }
#define RETURNLIST(list,len) \
{ Tcl_SetObjResult(ip,Tcl_ListFromArray(len,list)); return TCL_OK; }
#define RETERR(errmsg) \
{ if (NULL != ip) Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; }

typedef enum { TEST, MAXPOW, TPMO, N, PRIMPOWS, RDEGS, EDEGS, INVERSE, BINOM, BINOM2 } ecmdcode;

static const char *eCmdNames[] = { "test", "maxpower", "tpmo", "NALG", "powers",
                                   "rdegrees", "edegrees", "inverse", "binom", "binom2",
                                   (char *) NULL };

static ecmdcode eCmdmap[] = { TEST, MAXPOW, TPMO, N, PRIMPOWS,
                              RDEGS, EDEGS, INVERSE, BINOM, BINOM2 };

#define EXPECTARGS(num,msg) \
 { if (objc != (num)) { Tcl_WrongNumArgs(ip, 3, objv, msg); return TCL_ERROR; } }

int PrimeCombiCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *const objv[])
 {
    int result, index, a, b;
    primeInfo *pi;

    if (objc < 3) {
        Tcl_WrongNumArgs(ip, 1, objv, "<prime> subcommand ?arg arg ...?");
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(ip, objv[2], eCmdNames, "subcommand", 0, &index);
    if (result != TCL_OK) return result;

    if (TEST != eCmdmap[index])
        if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[1], &pi))
            return TCL_ERROR;

    switch (eCmdmap[index]) {
        case TEST:
            EXPECTARGS(3,"");
            Tcl_GetPrimeInfo(ip, objv[1], &pi);
            return TCL_OK;

        case MAXPOW:
            EXPECTARGS(3,"");
            RETURNINT(pi->maxpowerXintI);

        case TPMO:
            EXPECTARGS(3,"");
            RETURNINT(pi->tpmo);

        case N:
            EXPECTARGS(3,"");
            RETURNINT(NALG);

        case PRIMPOWS:
            EXPECTARGS(3,"");
            RETURNLIST((int*)pi->primpows, NALG);

        case RDEGS:
            EXPECTARGS(3,"");
            RETURNLIST((int*)pi->reddegs, NALG);

        case EDEGS:
            EXPECTARGS(3,"");
            RETURNLIST((int*)pi->extdegs, NALG);

        case INVERSE:
            EXPECTARGS(4,"<integer>");
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &a))
                return TCL_ERROR;
            a %= pi->prime;
            if (0 == a)
                RETERR("division by zero");
            RETURNINT(pi->inverse[a]);

        case BINOM:
            EXPECTARGS(5,"<integer> <integer>");
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &a))
                return TCL_ERROR;
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &b))
                return TCL_ERROR;
            RETURNINT(binomp(pi, a, b));
        case BINOM2:
            EXPECTARGS(5,"<integer> <integer>");
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &a))
                return TCL_ERROR;
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &b))
                return TCL_ERROR;
	    {
		int col, bin = binomp2(pi, a, b, &col);
		Tcl_Obj *ob[2];
		ob[0] = Tcl_NewIntObj(bin);
		ob[1] = Tcl_NewIntObj(col);
		Tcl_SetObjResult(ip,Tcl_NewListObj(2,ob));
	    }
	    return TCL_OK;
    }

    Tcl_SetResult(ip, "internal error in PrimeCombiCmd", TCL_STATIC);
    return TCL_ERROR;
}

/* List of primeInfo structures that have been constructed */
typedef struct piList {
    primeInfo pi;
    struct piList *next;
} piList;

static piList *piMasterList;

TCL_DECLARE_MUTEX(primeMutex)

/* return values come from makePrimeInfo */
int findPrimeInfo(int prime, primeInfo **pi) {
    piList **nextp = &(piMasterList);
    int rcode;
    for (; NULL != *nextp; nextp = &((*nextp)->next))
        if (prime == (*nextp)->pi.prime) { *pi = &((*nextp)->pi); return PI_OK; }
    Tcl_MutexLock(&primeMutex);
    if (NULL == ((*nextp) = (piList*)mallox(sizeof(piList)))) {
	rcode = PI_NOMEM;
    } else {
	if (PI_OK != (rcode = makePrimeInfo(&((*nextp)->pi), prime))) {
	    freex(*nextp);
	    *nextp = NULL;
	} else {
	    (*nextp)->next = NULL;
	    *pi = &((*nextp)->pi);
	    rcode = PI_OK;
	}
    }
    Tcl_MutexUnlock(&primeMutex);
    return rcode;
}

static Tcl_ObjType PrimeType;

/* try to turn objPtr into a PrimeType object */
int PT_SetFromAnyProc(Tcl_Interp *interp, Tcl_Obj *objPtr) {
    int rc, prime;
    primeInfo *pi;
    DBGPR printf("PT_SetFromAnyProc obj at %p\n",objPtr);
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
    TRYFREEOLDREP(objPtr);
    objPtr->typePtr = &PrimeType;
    objPtr->internalRep.twoPtrValue.ptr1 = (void *) pi;
    return TCL_OK;
}

/* recreate string representation */
void PT_UpdateStringProc(Tcl_Obj *objPtr) {
    primeInfo *pi = (primeInfo *) objPtr->internalRep.twoPtrValue.ptr1;
    DBGPR printf("PT_UpdateStringProc obj at %p\n",objPtr);
    objPtr->bytes = (char*) ckalloc(5);
    sprintf(objPtr->bytes, "%d", pi->prime);
    objPtr->length = strlen(objPtr->bytes);
}

/* create copy */
void PT_DupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    DBGPR printf("PT_DupInternalRepProc %p -> %p\n",srcPtr,dupPtr);
    dupPtr->typePtr = &PrimeType;
    dupPtr->internalRep.twoPtrValue.ptr1 = srcPtr->internalRep.twoPtrValue.ptr1;
}

int Tcl_GetPrimeInfo(Tcl_Interp *ip, Tcl_Obj *obj, primeInfo **pi) {
    if (TCL_OK != Tcl_ConvertToType(ip, obj, &PrimeType)) return TCL_ERROR;
    *pi = (primeInfo *) obj->internalRep.twoPtrValue.ptr1;
    return TCL_OK;
}

/* our namespace */
#define NSP "steenrod::"

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

    Tcl_CreateObjCommand(ip, NSP "prime", PrimeCombiCmd, (ClientData) 0, NULL);

    return TCL_OK;
}
