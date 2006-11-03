/*
 * Scriptable Tcl-Object support (scrobjy)
 *
 * Copyright (C) 2006 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This file is hereby released into the Public Domain.
 * To view a copy of the public domain dedication, visit
 *
 *     http://creativecommons.org/licenses/publicdomain/
 *
 * or send a letter to Creative Commons, 559 Nathan Abbott Way,
 * Stanford, California 94305, USA.
 */

#include <tcl.h>

#include <string.h>
#include <stdlib.h>

int Scrobjy_Init(Tcl_Interp *ip);

Tcl_Obj *ObjStringCopy(Tcl_Obj *other) {
    int length;
    char *bytes;
    bytes = Tcl_GetStringFromObj(other,&length);
    return Tcl_NewStringObj(bytes,length);
}

/* We use the following extension of Tcl's Tcl_Obj struct
 * to describe each scripted type that has been declared. */

typedef struct {
    Tcl_ObjType TclType;
    Tcl_Interp *ip;      /* where to evaluate the scripts */
    Tcl_Obj *Apply;      /* "::apply" */
    Tcl_Obj *UpdateCode; /* lambdaExpr to re-create string rep. */
    Tcl_Obj *SetAnyCode; /* lambdaExpr to parse from string */
    Tcl_Obj *FreeCode;   /* destructor as lambdaExpr */
} ScrObjType;

/* It can happen that the update & free code of a type is 
 * changed when there are still instances for the old code
 * around. For this reason each instance must keep its own
 * reference to its original lambdaExpressions. */

typedef struct {
    Tcl_Obj *updcode;
    Tcl_Obj *freecode;
} ScrObjInstData;

/* We use the two-pointer form of the instance data for the 
 * internal representation and the instance data: */

#define INTREPPTR(optr) ((optr)->internalRep.twoPtrValue.ptr1)
#define INTREPOBJ(optr) ((Tcl_Obj *) INTREPPTR(optr))

#define INSTDATAPTR(optr) ((optr)->internalRep.twoPtrValue.ptr2)
#define INSTDATASTRUCT(optr) ((ScrObjInstData *) INSTDATAPTR(optr))
#define UPDATECODE(optr) ((INSTDATASTRUCT(optr))->updcode)
#define FREECODE(optr)   ((INSTDATASTRUCT(optr))->freecode)

ScrObjInstData *NewInstData(ScrObjType *tp) {
    ScrObjInstData *res = 
        (ScrObjInstData *) Tcl_Alloc(sizeof(ScrObjInstData));
    if (NULL == res) return NULL;
    res->updcode = tp->UpdateCode;
    res->freecode = tp->FreeCode;
    Tcl_IncrRefCount(res->updcode);
    Tcl_IncrRefCount(res->freecode);
    return res;
}

ScrObjInstData *DupInstData(ScrObjInstData *id) {
    ScrObjInstData *res = 
        (ScrObjInstData *) Tcl_Alloc(sizeof(ScrObjInstData));
    if (NULL == res) return NULL;
    res->updcode = id->updcode;
    res->freecode = id->freecode;
    Tcl_IncrRefCount(res->updcode);
    Tcl_IncrRefCount(res->freecode);
    return res;
    
}

void FreeInstData(ScrObjInstData *id) {
    Tcl_DecrRefCount(id->updcode);
    Tcl_DecrRefCount(id->freecode);
    Tcl_Free((char *) id);
}

/* implementation of the Tcl_Obj interface functions */

int ScrObjTypeEvalCode(ScrObjType *tp, Tcl_Obj *code, Tcl_Obj *arg) {
    Tcl_Obj *aux[3];
    int rc;
    aux[0] = tp->Apply;
    aux[1] = code;
    aux[2] = arg;
    Tcl_IncrRefCount(aux[0]);
    Tcl_IncrRefCount(aux[1]);
    Tcl_IncrRefCount(aux[2]);
    rc = Tcl_EvalObjv(tp->ip,3,aux,TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(aux[0]);
    Tcl_DecrRefCount(aux[1]);
    Tcl_DecrRefCount(aux[2]);
    return rc;
}

void ScrobjyFreeproc(Tcl_Obj *objPtr) {
    ScrObjType *tp = (ScrObjType *) objPtr->typePtr;
    Tcl_Obj *freecode = FREECODE(objPtr);
    if (*Tcl_GetString(freecode)) {
        ScrObjTypeEvalCode(tp,freecode,INTREPOBJ(objPtr));
    }
    Tcl_DecrRefCount(INTREPOBJ(objPtr));
    FreeInstData(INSTDATAPTR(objPtr));
}

void ScrobjUpdateproc (Tcl_Obj *objPtr) {
    Tcl_Obj *res;
    char *sres; int slen;
    ScrObjType *tp = (ScrObjType *) objPtr->typePtr;
    ScrObjTypeEvalCode(tp,UPDATECODE(objPtr),INTREPOBJ(objPtr));
    res = Tcl_GetObjResult(tp->ip);
    sres = Tcl_GetStringFromObj(res,&slen);
    objPtr->bytes = Tcl_Alloc(slen+1);
    memcpy(objPtr->bytes,sres,slen);
    objPtr->bytes[slen]=0;
    objPtr->length = slen;
}

int ScrobjSetproc(ScrObjType *tp, Tcl_Interp *ip, Tcl_Obj *objPtr) {
    Tcl_Obj *res;
    int rc;
    rc = ScrObjTypeEvalCode(tp,tp->SetAnyCode,objPtr);
    res = Tcl_GetObjResult(tp->ip);
    if (TCL_OK == rc) {
        ScrObjInstData *id = NewInstData(tp);
        if (NULL == id) {
            Tcl_SetResult(ip, "out of memory", TCL_STATIC);
            return TCL_ERROR;
        }
        if (objPtr->typePtr && objPtr->typePtr->freeIntRepProc) {
            objPtr->typePtr->freeIntRepProc(objPtr);
        }
        objPtr->typePtr = (Tcl_ObjType *) tp;
        INTREPPTR(objPtr) = res;
        Tcl_IncrRefCount(res);
        INSTDATAPTR(objPtr) = id;
        return TCL_OK;
    } else {
        Tcl_AddErrorInfo(ip,Tcl_GetVar(tp->ip,"::errorInfo",0));
    }
    Tcl_SetResult(ip, Tcl_GetString(res), TCL_VOLATILE);
    return rc;
}

void ScrobjyDupproc (Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    if(dupPtr->typePtr && dupPtr->typePtr->freeIntRepProc) {
        dupPtr->typePtr->freeIntRepProc(dupPtr);
    }
    dupPtr->typePtr = srcPtr->typePtr;
    INTREPPTR(dupPtr) = INTREPPTR(srcPtr);
    Tcl_IncrRefCount(INTREPOBJ(dupPtr));
    INSTDATAPTR(dupPtr) = DupInstData(INSTDATASTRUCT(srcPtr));
}

Tcl_Obj *NewScrobj(ScrObjType *tp, Tcl_Obj *intrep) {
    Tcl_Obj *res = Tcl_NewObj();
    ScrObjInstData *id = NewInstData(tp);
    if (NULL == res || NULL == id) {
        return NULL;
    }
    res->bytes = NULL;
    res->typePtr = (Tcl_ObjType *) tp;
    INTREPPTR(res) = intrep;
    Tcl_IncrRefCount(intrep);
    INSTDATAPTR(res) = id;
    return res;
}

ScrObjType *NewScrobjType(Tcl_Interp *ip, Tcl_Obj *nameobj, 
                          Tcl_Obj *update, Tcl_Obj *setany, 
                          Tcl_Obj *freecode) {
    ScrObjType *res = (ScrObjType *) ckalloc(sizeof(ScrObjType));
    char aux[200];
    if (NULL == res) return NULL;
    snprintf(aux,190,"scrobjy type %s",Tcl_GetString(nameobj));
    aux[190]=0;
    res->TclType.name = strdup(aux);
    res->TclType.freeIntRepProc = ScrobjyFreeproc;
    res->TclType.dupIntRepProc = ScrobjyDupproc;
    res->TclType.updateStringProc = ScrobjUpdateproc;
    res->TclType.setFromAnyProc = NULL;
    res->Apply = Tcl_NewStringObj("::apply",-1);
    res->UpdateCode = update;
    res->SetAnyCode = setany;
    res->FreeCode = freecode;
    Tcl_IncrRefCount(res->Apply);
    Tcl_IncrRefCount(res->UpdateCode);
    Tcl_IncrRefCount(res->SetAnyCode);
    Tcl_IncrRefCount(res->FreeCode);
    res->ip = Tcl_CreateInterp();
    /* make sure our slave has the same Tcl environment */
    Tcl_SetVar(res->ip,"::tcl_library",
               Tcl_GetVar(ip,"::tcl_library",0),0);
    Tcl_Init(res->ip);
    Tcl_SetAssocData(ip,"scrobjy",NULL,Tcl_GetAssocData(ip,"scrobjy",NULL));
    Scrobjy_Init(res->ip);
    return res;
}

int ScrObjTNSetProc(Tcl_Interp *interp, Tcl_Obj *objPtr);
void ScrObjTNDupProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);

Tcl_ObjType ScrobjTypeName = {
    "scrobjy type name",
    NULL,
    ScrObjTNDupProc,
    NULL,
    NULL
};

int MakeTypeNameObj(Tcl_HashTable *TypeTablePtr,
                    Tcl_Interp *interp, Tcl_Obj *objPtr) {
    Tcl_HashEntry *res = Tcl_FindHashEntry(TypeTablePtr, (void *) objPtr);
    if (NULL == res) {
        if(NULL != interp) {
            Tcl_AppendResult(interp, "Type ", Tcl_GetString(objPtr), " not found", NULL);
        }
        return TCL_ERROR;
    }
    if (objPtr->typePtr && objPtr->typePtr->freeIntRepProc) {
        objPtr->typePtr->freeIntRepProc(objPtr);
    }
    objPtr->typePtr = (Tcl_ObjType *) &ScrobjTypeName;
    INTREPPTR(objPtr) = Tcl_GetHashValue(res);
    return TCL_OK;
}

void ScrObjTNDupProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    dupPtr->typePtr = (Tcl_ObjType *) &ScrobjTypeName;
    INTREPPTR(dupPtr) = INTREPPTR(srcPtr);
}

typedef enum { REGISTER, CONVERT, VALUE, EVAL, GETSTRING } ScrobjCmdCode;

static CONST char *ScrobjCmdNames[] = { "register", "convert", "value", "eval", "getstring", NULL };

static ScrobjCmdCode ScrobjCmdmap[] = { REGISTER, CONVERT, VALUE, EVAL, GETSTRING };

int ScrobjyCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[]) {
    int index, result;
    Tcl_HashTable *TypeTablePtr = (Tcl_HashTable *) cd;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(ip, objv[1], ScrobjCmdNames, "subcommand", 0, &index);
    if (result != TCL_OK) return result;

    switch (ScrobjCmdmap[index]) {
        case REGISTER:
        {
            ScrObjType *tp;
            Tcl_Obj *ucode, *scode, *fcode;

            if (objc > 6 || objc < 5) {
                Tcl_WrongNumArgs(ip, 2, objv, "typename updateCode setFromAnyCode ?freeCode?");
                return TCL_ERROR;
            }

            ucode = objv[3];
            scode = objv[4];
            fcode = (objc > 5) ? objv[5] : Tcl_NewObj();

            if (TCL_OK != MakeTypeNameObj(TypeTablePtr,NULL,objv[2])) {
                /* create new type entry */

                int isnew;
                Tcl_HashEntry *res;

                res = Tcl_CreateHashEntry(TypeTablePtr, (void *) objv[2], &isnew);

                Tcl_SetHashValue(res, NewScrobjType(ip,objv[2],ucode,scode,fcode));

            } else {
                /* update existing entry */

#define SETNEWVAL(old,new) { \
    oldval=old;              \
    old=new;                 \
    Tcl_IncrRefCount(new);   \
    Tcl_DecrRefCount(old); }

                Tcl_Obj *oldval;
                tp = (ScrObjType *) objv[2]->internalRep.twoPtrValue.ptr1;

                SETNEWVAL(tp->UpdateCode,ucode);
                SETNEWVAL(tp->SetAnyCode,scode);
                SETNEWVAL(tp->FreeCode,fcode);
            }
            return TCL_OK;
        }
        case CONVERT:
        {
            ScrObjType *tp;
            if (objc != 4) {
                Tcl_WrongNumArgs(ip, 2, objv, "typename value");
                return TCL_ERROR;
            }

            if (TCL_OK != MakeTypeNameObj(TypeTablePtr,ip,objv[2])) {
                return TCL_ERROR;
            }

            tp = (ScrObjType *) objv[2]->internalRep.twoPtrValue.ptr1;

            if(objv[3]->typePtr != (Tcl_ObjType *) tp) {
                if (TCL_OK != ScrobjSetproc(tp, ip, objv[3])) {
                    return TCL_ERROR;
                }
            }

            Tcl_SetObjResult(ip, objv[3]->internalRep.twoPtrValue.ptr1);
            return TCL_OK;
        }
        case EVAL:
        {
            ScrObjType *tp;
	    int rc;

            if (objc != 4) {
                Tcl_WrongNumArgs(ip, 2, objv, "typename script");
                return TCL_ERROR;
            }

            if (TCL_OK != MakeTypeNameObj(TypeTablePtr,ip,objv[2])) {
                return TCL_ERROR;
            }

            tp = (ScrObjType *) objv[2]->internalRep.twoPtrValue.ptr1;

	    rc = Tcl_EvalObjEx(tp->ip,objv[3],TCL_EVAL_GLOBAL);
	    if (TCL_OK != rc) {
	       Tcl_AddErrorInfo(ip,Tcl_GetVar(tp->ip,"::errorInfo",0));
	    }
            Tcl_SetObjResult(ip,Tcl_GetObjResult(tp->ip));
            return rc;
	}
        case GETSTRING:
        {
            int length; char *bytes;
            if (objc != 3) {
                Tcl_WrongNumArgs(ip, 2, objv, "value");
                return TCL_ERROR;
            }

            bytes = Tcl_GetStringFromObj(objv[2],&length);
            Tcl_SetObjResult(ip,Tcl_NewStringObj(bytes,length));
            return TCL_OK;
        }
        case VALUE:
        {
            if (objc != 4) {
                Tcl_WrongNumArgs(ip, 2, objv, "typename internal-rep");
                return TCL_ERROR;
            }

            if (TCL_OK != MakeTypeNameObj(TypeTablePtr,ip,objv[2])) {
                return TCL_ERROR;
            }

            ScrObjType *tp = (ScrObjType *) INTREPPTR(objv[2]);

            Tcl_SetObjResult(ip, NewScrobj(tp,objv[3]));
            return TCL_OK;
        }
    }
    return TCL_ERROR;
}

int Scrobjy_Init(Tcl_Interp *ip) {

    Tcl_HashTable *TypeTablePtr;

    Tcl_InitStubs(ip, "8.0", 0);

    TypeTablePtr = (Tcl_HashTable *) Tcl_GetAssocData(ip,"scrobjy",NULL);

    if (NULL == TypeTablePtr) {
        TypeTablePtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitObjHashTable(TypeTablePtr);
        Tcl_SetAssocData(ip,"scrobjy",NULL,(ClientData) TypeTablePtr);
    }

    Tcl_CreateObjCommand(ip, "scrobjy", ScrobjyCmd, 
                         (ClientData) TypeTablePtr, NULL);
    Tcl_Eval(ip,"package provide scrobjy 1.0");

    return TCL_OK;
}

