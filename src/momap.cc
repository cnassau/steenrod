/*
 * Monomaps - implements a map from monomials to Tcl_Objects
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

#define MOMAC

#include <tcl.h>
#include "setresult.h"
#include <string.h>
#include "tprime.h"
#include "tpoly.h"
#include "momap.h"

#define LOGDATA 0

static Tcl_HashKeyType MomaHashType;

/* cast from (Tcl_HashEntry *) to our (const exmo *) key */
#define keyFromHE(he) ((Tcl_Obj *) (he)->key.objPtr)

unsigned int momaHashProc(Tcl_HashTable *table, void *key) {
    exmo *ex = exmoFromTclObj((Tcl_Obj *) key);
    int res, cnt;

    res = ex->gen + 17*(ex->ext);
    for (cnt=0; cnt<NALG; cnt++)
    res += (ex->r.dat[cnt]) << (3*cnt);

    return res;
}

int momaCompProc(void *keyPtr, Tcl_HashEntry *hPtr) {
    exmo *e1 = exmoFromTclObj((Tcl_Obj *) keyPtr);
    exmo *e2 = exmoFromTclObj((Tcl_Obj *) keyFromHE(hPtr));

    if (e1 == e2) return 1;

    return compareExmo(e1,e2) ? 0 : 1;
}

momap *momapCreate(void) {
    momap *res = (momap*) mallox(sizeof(momap));
    if (NULL == res) return NULL;

    if (NULL == (res->tab = (Tcl_HashTable*) mallox(sizeof(Tcl_HashTable)))) {
    freex(res);
    return NULL;
    }

    Tcl_InitCustomHashTable(res->tab, TCL_CUSTOM_PTR_KEYS, &MomaHashType);
    return res;
}

void freeValues(momap *mo) {
    Tcl_HashSearch src;
    Tcl_HashEntry *ent = Tcl_FirstHashEntry(mo->tab, &src);
    while (NULL != ent) {
    Tcl_Obj *obj = (Tcl_Obj *) Tcl_GetHashValue(ent);
    DECREFCNT(obj);
    DECREFCNT(keyFromHE(ent));
    ent = Tcl_NextHashEntry(&src);
    }
    Tcl_DeleteHashTable(mo->tab);
}

void momapClear(momap *mo) {
    freeValues(mo);
    Tcl_InitCustomHashTable(mo->tab, TCL_CUSTOM_PTR_KEYS, &MomaHashType);
}

void momapDestroy(momap *mo) {
    freeValues(mo);
    freex(mo->tab);
}

Tcl_Obj *momapGetValPtr(momap *mo, Tcl_Obj *key) {
    Tcl_HashEntry *ent = Tcl_FindHashEntry(mo->tab, (void *) key);
    if (NULL == ent) return NULL;
    return (Tcl_Obj *) Tcl_GetHashValue(ent);
}

int momapRemoveValue(momap *mo, Tcl_Obj *key) {
    Tcl_HashEntry *ent;
    Tcl_Obj *val;
    if (NULL == (ent = Tcl_FindHashEntry(mo->tab, (void *) key)))
    return SUCCESS;
    val = (Tcl_Obj*) Tcl_GetHashValue(ent);
    if (NULL != val) DECREFCNT(val);
    /* NOTE: "keyFromHE(ent)" and "key" can be different Tcl_Obj'ects (!) */
    DECREFCNT(keyFromHE(ent));
    Tcl_DeleteHashEntry(ent);
    return SUCCESS;
}

int momapSetValPtr(momap *mo, Tcl_Obj *key, Tcl_Obj *val) {
    int newFlag;
    Tcl_HashEntry *ent = Tcl_CreateHashEntry(mo->tab, (void *) key, &newFlag);

    if (!newFlag) {
    Tcl_Obj *aux = (Tcl_Obj*)  Tcl_GetHashValue(ent);
    if (NULL != aux) DECREFCNT(aux);
    } else {
    INCREFCNT(key);
    }

    Tcl_SetHashValue(ent, (ClientData) val);
    INCREFCNT(val);

    return SUCCESS;
}

/**** TCL INTERFACE **********************************************************/

#define RETERR(errmsg) \
{ if (NULL != ip) Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; }

typedef enum { CLEAR, SET, GET, LIST, UNSET, ADD, APPEND } momacmdcode;

static const char *cmdNames[] = { "clear", "set", "get", "list", "unset", "add", "append",
                                  (char *) NULL };

static momacmdcode cmdmap[] = { CLEAR, SET, GET, LIST, UNSET, ADD, APPEND };

int Tcl_MomaWidgetCmd(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * const objv[]) {
    momap *mo = (momap *) cd;
    int result, index;
    int scale, modulo;
    Tcl_Obj *auxptr;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(ip, objv[1], cmdNames, "subcommand", 0, &index);
    if (result != TCL_OK) return result;

    switch (cmdmap[index]) {
        case CLEAR:
            if (objc != 2) {
                Tcl_WrongNumArgs(ip, 2, objv, NULL);
                return TCL_ERROR;
            }
            momapClear(mo);
            return TCL_OK;

        case SET:
            if (objc != 4) {
                Tcl_WrongNumArgs(ip, 2, objv, "<monomial> value");
                return TCL_ERROR;
            }
            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
                return TCL_ERROR;
            result = momapSetValPtr(mo, objv[2], objv[3]);
            if (TCL_OK != result) RETERR("out of memory");
            return TCL_OK;

        case ADD:
        case APPEND:
            scale = 1; modulo = 0;
            if ((objc<4) || (objc>6)) {
                Tcl_WrongNumArgs(ip, 2, objv,
                                 "<monomial> <polynomial> ?scale? ?modulo?");
                return TCL_ERROR;
            }
            if (objc>4)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &scale))
                    return TCL_ERROR;
            if (objc>5)
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[5], &modulo))
                    return TCL_ERROR;
            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
                return TCL_ERROR;
            auxptr = momapGetValPtr(mo, objv[2]);
            if (NULL == auxptr) {
                result = momapSetValPtr(mo, objv[2], objv[3]);
                if (SUCCESS != result) RETERR("out of memory");
            } else {
                if (TCL_OK != Tcl_ConvertToPoly(ip, auxptr))
                    RETERR("current value not of polynomial type");
                if (TCL_OK != Tcl_ConvertToPoly(ip, objv[3]))
                    return TCL_ERROR;
                if (Tcl_IsShared(auxptr)) {
                    auxptr= Tcl_DuplicateObj(auxptr);
                    momapSetValPtr(mo, objv[2], auxptr);
                }
                Tcl_InvalidateStringRep(auxptr);
                if (SUCCESS != PLappendPoly(polyTypeFromTclObj(auxptr),
                                            polyFromTclObj(auxptr),
                                            polyTypeFromTclObj(objv[3]),
                                            polyFromTclObj(objv[3]),
                                            NULL, 0,
                                            scale,modulo))
                    return TCL_ERROR;

                if (cmdmap[index] != APPEND) {
                    if (SUCCESS != PLcancel(polyTypeFromTclObj(auxptr),
                                            polyFromTclObj(auxptr),
                                            modulo))
                        return TCL_ERROR;
                }
                return TCL_OK;
            }
            return TCL_OK;
        case GET:
            if (objc != 3) {
                Tcl_WrongNumArgs(ip, 2, objv, "<monomial>");
                return TCL_ERROR;
            }
            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
                return TCL_ERROR;
            auxptr = momapGetValPtr(mo, objv[2]);
            if (NULL == auxptr) {
                Tcl_SetObjResult(ip, Tcl_NewObj());
                return TCL_OK;
            }
            Tcl_SetObjResult(ip, auxptr);
            return TCL_OK;

        case LIST:
            if (objc != 2) {
                Tcl_WrongNumArgs(ip, 2, objv, "");
                return TCL_ERROR;
            }
            {
                Tcl_Obj *res = Tcl_NewObj(), *key, *val;
                Tcl_HashSearch src;
                Tcl_HashEntry *ent = Tcl_FirstHashEntry(mo->tab, &src);
                while (NULL != ent) {
                    val = (Tcl_Obj *) Tcl_GetHashValue(ent);
                    key = keyFromHE(ent);
                    Tcl_ListObjAppendElement(ip, res, key);
                    Tcl_ListObjAppendElement(ip, res, val);
                    ent = Tcl_NextHashEntry(&src);
                }
                Tcl_SetObjResult(ip, res);
                return TCL_OK;
            }

        case UNSET:
            if (objc != 3) {
                Tcl_WrongNumArgs(ip, 2, objv, "<monomial>");
                return TCL_ERROR;
            }
            if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
                return TCL_ERROR;
            momapRemoveValue(mo, objv[2]);
            return TCL_OK;
    }

    RETERR("internal error in Tcl_MomaWidgetCmd");
}

void Tcl_DestroyMoma(ClientData cd) {
    momap *mo = (momap *) cd;
    momapDestroy(mo);
}

momap *Tcl_MomapFromObj(Tcl_Interp *ip, Tcl_Obj *obj) {
    Tcl_CmdInfo info;

    if (!Tcl_GetCommandInfo(ip, Tcl_GetString(obj), &info)) {
        Tcl_SetResult(ip, "command not found", TCL_STATIC);
        return NULL;
    }

    if (info.objProc != Tcl_MomaWidgetCmd) {
        Tcl_SetResult(ip, "monomap object expected", TCL_STATIC);
        return NULL;
    }

    return (momap *) info.objClientData;
}


int Tcl_CreateMomaCmd(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * const objv[]) {
    momap *mo;

    if (objc != 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "name");
        return TCL_ERROR;
    }

    if (NULL == (mo = momapCreate())) RETERR("out of memory");

    Tcl_CreateObjCommand(ip, Tcl_GetString(objv[1]),
                         Tcl_MomaWidgetCmd, (ClientData) mo, Tcl_DestroyMoma);

    return TCL_OK;
}

int Momap_Init(Tcl_Interp *ip) {

    Tcl_InitStubs(ip, "8.0", 0);

    Tptr_Init(ip);
    Tprime_Init(ip);
    Tpoly_Init(ip);

    Tcl_CreateObjCommand(ip, POLYNSP "monomap",
                         Tcl_CreateMomaCmd, (ClientData) 0, NULL);


    MomaHashType.version = TCL_HASH_KEY_TYPE_VERSION;
    MomaHashType.flags = 0;
    MomaHashType.hashKeyProc = (Tcl_HashKeyProc*) momaHashProc;
    MomaHashType.compareKeysProc = momaCompProc;
    MomaHashType.allocEntryProc = NULL; /* momaAllocProc; */
    MomaHashType.freeEntryProc = NULL; /* momaFreeProc; */

    return TCL_OK;
}
