/*
 * Monomaps - implements a map from monomials to Tcl_Objects
 *
 * Copyright (C) 2004 Christian Nassau <nassau@nullhomotopie.de>
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
#include <string.h>
#include "tprime.h"
#include "tpoly.h"
#include "momap.h"

#define LOGDATA 0

momap *momapCreate(void) {
    momap *res = mallox(sizeof(momap));
    if (NULL == res) return NULL;
    if (NULL == (res->keys = stdpoly->createCopy(NULL))) {
        freex(res); return NULL; 
    }
    res->values = NULL;
    res->valloc = 0;
    return res;
}

void freeValues(momap *mo) {
    if (NULL != mo->values) { 
        int i, len = stdpoly->getNumsum(mo->keys);
        for (i=0;i<len;i++)
            DECREFCNT(mo->values[i]);
        freex(mo->values); mo->values = NULL; mo->valloc = 0;
    }
}

void momapClear(momap *mo) {
    freeValues(mo);
    stdpoly->clear(mo->keys);
}

void momapDestroy(momap *mo) {
    freeValues(mo);
    stdpoly->free(mo->keys);
    freex(mo);
}

Tcl_Obj **momapGetValPtr(momap *mo, const exmo *key) {
    int idx = stdpoly->lookup(mo->keys, key, NULL);
    Tcl_Obj **res;
    if (idx < 0) res = NULL;
    else res = &(mo->values[idx]);
    if (LOGDATA) printf("momapGetValPtr[%d] => %p (points to %p)\n", 
                        idx, res, (NULL != res) ? *res : NULL);
    return res;
}

int momapRemoveValue(momap *mo, const exmo *key) {
    int idx = stdpoly->lookup(mo->keys, key, NULL), len;
    if (idx < 0) return FAILIMPOSSIBLE;
    stdpoly->remove(mo->keys,idx);  
    len = stdpoly->getNumsum(mo->keys);
    memmove(&(mo->values[idx]), &(mo->values[idx+1]), sizeof(Tcl_Obj *) * (len-idx));
    return SUCCESS;
}

#define MOMAPSTEPSIZE 100 

int momapSetValPtr(momap *mo, const exmo *key, Tcl_Obj *val) {
    Tcl_Obj **aux = momapGetValPtr(mo, key);
    if (LOGDATA) printf("momapGetValPtr set val %p\n",val);
    INCREFCNT(val);
    if (NULL != aux) {
        DECREFCNT(*aux);
        *aux = val;
    } else {
        int len = stdpoly->getNumsum(mo->keys);
        if (mo->valloc < len+1) {
            void *newptr = reallox(mo->values, 
                                   (len + MOMAPSTEPSIZE) * sizeof(Tcl_Obj *));
            if (NULL == newptr) { 
                DECREFCNT(val);
                return FAILMEM; 
            }
            mo->valloc = len + MOMAPSTEPSIZE; 
            mo->values = (Tcl_Obj **) newptr;
        }
        mo->values[len] = val;
        stdpoly->appendExmo(mo->keys, key);
        if (SUCCESS != stdpolySortWithValues(mo->keys, (void **) mo->values)) {
            momapClear(mo);
            return FAILMEM;
        }
    }
    return SUCCESS;
}

/**** TCL INTERFACE **********************************************************/

#define RETERR(errmsg) \
{ if (NULL != ip) Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; }

typedef enum { CLEAR, SET, GET, LIST, UNSET, ADD } momacmdcode;

static CONST char *cmdNames[] = { "clear", "set", "get", "list", "unset", "add",
                                  (char *) NULL };

static momacmdcode cmdmap[] = { CLEAR, SET, GET, LIST, UNSET, ADD };

int Tcl_MomaWidgetCmd(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * const objv[]) {
    momap *mo = (momap *) cd;
    int result, index;
    int scale, modulo;
    exmo *ex;
    Tcl_Obj **auxptr;

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
           ex = exmoFromTclObj(objv[2]);
           result = momapSetValPtr(mo, ex, objv[3]);
           if (TCL_OK != result) RETERR("out of memory");
           return TCL_OK;

       case ADD:
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
           ex = exmoFromTclObj(objv[2]);
           auxptr = momapGetValPtr(mo, ex);
           if (NULL == auxptr) {
               result = momapSetValPtr(mo, ex, objv[3]);
               if (TCL_OK != result) RETERR("out of memory");
           } else {
               if (TCL_OK != Tcl_ConvertToPoly(ip, *auxptr))
                   RETERR("current value not of polynomial type");
               if (TCL_OK != Tcl_ConvertToPoly(ip, objv[3]))
                   return TCL_ERROR;
               if (Tcl_IsShared(*auxptr)) {
                   DECREFCNT(*auxptr);
                   *auxptr = Tcl_DuplicateObj(*auxptr);
                   INCREFCNT(*auxptr);
               }
               Tcl_InvalidateStringRep(*auxptr);
               if (SUCCESS != PLappendPoly(polyTypeFromTclObj(*auxptr),
                                           polyFromTclObj(*auxptr),
                                           polyTypeFromTclObj(objv[3]),
                                           polyFromTclObj(objv[3]),
                                           NULL, 0,
                                           scale,modulo))
                   return TCL_ERROR;
               if (SUCCESS != PLcancel(polyTypeFromTclObj(*auxptr),
                                       polyFromTclObj(*auxptr),
                                       modulo))
                   return TCL_ERROR;
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
           ex = exmoFromTclObj(objv[2]);
           auxptr = momapGetValPtr(mo, ex);
           if (NULL == auxptr) {
               Tcl_SetObjResult(ip, Tcl_NewObj());
               return TCL_OK;
           }
           Tcl_SetObjResult(ip, *auxptr);
           return TCL_OK;

       case LIST:
           if (objc != 2) {
               Tcl_WrongNumArgs(ip, 2, objv, "");
               return TCL_ERROR;
           }
           {
               int i, len = stdpoly->getNumsum(mo->keys);
               Tcl_Obj **aux = mallox(sizeof(Tcl_Obj *) * len * 2);
               if (NULL == aux) RETERR("out of memory");
               for (i=0;i<len;i++) {
                   exmo *exm; 
                   stdpoly->getExmoPtr(mo->keys, &exm, i);
                   aux[i<<1] = Tcl_NewExmoCopyObj(exm);
                   aux[(i<<1)|1] = mo->values[i];
               }
               Tcl_SetObjResult(ip, Tcl_NewListObj(2*len,aux));
               freex(aux);
               return TCL_OK;
           }

       case UNSET:
           if (objc != 3) {
               Tcl_WrongNumArgs(ip, 2, objv, "<monomial>");
               return TCL_ERROR;
           }
           if (TCL_OK != Tcl_ConvertToExmo(ip, objv[2]))
               return TCL_ERROR;
           ex = exmoFromTclObj(objv[2]);
           momapRemoveValue(mo, ex);
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
                      int objc, Tcl_Obj * CONST objv[]) {
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
    
    return TCL_OK;
}
