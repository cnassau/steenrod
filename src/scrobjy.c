/*
 * Scriptable Tcl-Object support (scrobjy)
 *
 * Copyright (C) 2004-2006 Christian Nassau <nassau@nullhomotopie.de>
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

#include "scrobjy.h"

#include <string.h>
#include <stdlib.h>

Tcl_HashTable TypeTable;

typedef struct {
   Tcl_ObjType TclType;
   Tcl_Interp *ip;
   Tcl_Obj *UpdateFunc;
   Tcl_Obj *SetAnyFunc;
} ScrObjType;

#define INTDATAPTR(optr) ((optr)->internalRep.twoPtrValue.ptr1)
#define INTDATAOBJ(optr) ((Tcl_Obj *) INTDATAPTR(optr))

int ScrobjSetproc (ScrObjType *tp, Tcl_Interp *ip, Tcl_Obj *objPtr) {
   Tcl_Obj *aux[2], *res;
   int rc; 
#ifdef SAVERES
   Tcl_SavedResult sr;
   Tcl_SaveResult(tp->ip,&sr);
#endif
   aux[0] = tp->SetAnyFunc;
   aux[1] = objPtr;
   Tcl_IncrRefCount(aux[0]);
   Tcl_IncrRefCount(aux[1]);
   rc = Tcl_EvalObjv(tp->ip,2,aux,TCL_EVAL_GLOBAL);
   Tcl_DecrRefCount(aux[0]);
   Tcl_DecrRefCount(aux[1]);
   res = Tcl_GetObjResult(tp->ip);
   if (TCL_OK == rc) {
      if (objPtr->typePtr && objPtr->typePtr->freeIntRepProc) {
	 objPtr->typePtr->freeIntRepProc(objPtr);
      }
      objPtr->typePtr = (Tcl_ObjType *) tp;
      INTDATAPTR(objPtr) = res;
      Tcl_IncrRefCount(res);
      return TCL_OK;
   }
   Tcl_SetResult(ip, Tcl_GetString(res), TCL_VOLATILE);
#ifdef SAVERES
reihenfolge falsch!
   Tcl_RestoreResult(tp->ip,&sr);
   Tcl_DiscardResult(&sr);
#endif
   return rc;
}

void ScrobjUpdateproc (Tcl_Obj *objPtr) {
   Tcl_Obj *aux[2], *res;
   char *sres; int slen;
   ScrObjType *tp = (ScrObjType *) objPtr->typePtr;
#ifdef SAVERES
   Tcl_SavedResult sr;
   Tcl_SaveResult(tp->ip,&sr);
#endif
   aux[0] = tp->UpdateFunc;
   aux[1] = INTDATAOBJ(objPtr);
   Tcl_IncrRefCount(aux[1]);
   Tcl_EvalObjv(tp->ip,2,aux,TCL_EVAL_GLOBAL);
   Tcl_DecrRefCount(aux[1]);
   res = Tcl_GetObjResult(tp->ip);
   sres = Tcl_GetStringFromObj(res,&slen);
   objPtr->bytes = Tcl_Alloc(slen+1);
   memcpy(objPtr->bytes,sres,slen);
   objPtr->bytes[slen]=0;
   objPtr->length = slen;
#ifdef SAVERES
   Tcl_RestoreResult(tp->ip,&sr);
   Tcl_DiscardResult(&sr);
#endif
}

void ScrobjyDupproc (Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
   if(dupPtr->typePtr && dupPtr->typePtr->freeIntRepProc) {
      dupPtr->typePtr->freeIntRepProc(dupPtr);
   }
   dupPtr->typePtr = srcPtr->typePtr;
   INTDATAPTR(dupPtr) = INTDATAPTR(srcPtr);
   Tcl_IncrRefCount(INTDATAOBJ(dupPtr));
}

void ScrobjyFreeproc (Tcl_Obj *objPtr) {
   Tcl_DecrRefCount(INTDATAOBJ(objPtr));
}


Tcl_Obj *NewScrobj(ScrObjType *tp, Tcl_Obj *intrep) {
   Tcl_Obj *res = Tcl_NewObj();
   res->bytes = NULL;
   res->typePtr = (Tcl_ObjType *) tp;
   INTDATAPTR(res) = intrep;
   Tcl_IncrRefCount(intrep);
   return res;
}

ScrObjType *NewScrobjType(Tcl_Interp *ip, Tcl_Obj *nameobj, Tcl_Obj *update, Tcl_Obj *setany) {
   ScrObjType *res = malloc(sizeof(ScrObjType));
   if (NULL == res) return NULL;
   char aux[200];
   sprintf(aux,"scrobjy %s",Tcl_GetString(nameobj));
   res->TclType.name = strdup(aux);
   res->TclType.freeIntRepProc = ScrobjyFreeproc;
   res->TclType.dupIntRepProc = ScrobjyDupproc;
   res->TclType.updateStringProc = ScrobjUpdateproc;
   res->TclType.setFromAnyProc = NULL;
   res->UpdateFunc = update;
   res->SetAnyFunc = setany;
   res->ip = ip;
   Tcl_IncrRefCount(res->UpdateFunc);
   Tcl_IncrRefCount(res->SetAnyFunc);
   return res;
}

int ScrObjTNSetProc(Tcl_Interp *interp, Tcl_Obj *objPtr);
void ScrObjTNDupProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);

Tcl_ObjType ScrobjTypeName = {
   "scrobjy type name",
   NULL,
   ScrObjTNDupProc,
   NULL,
   ScrObjTNSetProc
};

int ScrObjTNSetProc(Tcl_Interp *interp, Tcl_Obj *objPtr) {
   Tcl_HashEntry *res = Tcl_FindHashEntry(&TypeTable, (void *) objPtr);
   if (NULL == res) {
      Tcl_AppendResult(interp, "Type ", Tcl_GetString(objPtr), " not found", NULL);
      return TCL_ERROR;
   }
   if (objPtr->typePtr && objPtr->typePtr->freeIntRepProc) {
      objPtr->typePtr->freeIntRepProc(objPtr);
   }
   objPtr->typePtr = (Tcl_ObjType *) &ScrobjTypeName;
   INTDATAPTR(objPtr) = Tcl_GetHashValue(res);
   return TCL_OK;
}

void ScrObjTNDupProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
   dupPtr->typePtr = (Tcl_ObjType *) &ScrobjTypeName;
   INTDATAPTR(dupPtr) = INTDATAPTR(srcPtr);
}

typedef enum { REGISTER, CONVERT, VALUE } ScrobjCmdCode;

static CONST char *ScrobjCmdNames[] = { "register", "convert", "value", NULL };

static ScrobjCmdCode ScrobjCmdmap[] = { REGISTER, CONVERT, VALUE };

int ScrobjyCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[]) {
   int index, result;

   if (objc < 2) {
      Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
      return TCL_ERROR;
   }

   result = Tcl_GetIndexFromObj(ip, objv[1], ScrobjCmdNames, "subcommand", 0, &index);
   if (result != TCL_OK) return result;

   switch (ScrobjCmdmap[index]) {
   case REGISTER:
      {
	 int isnew;
	 if (objc != 5) {
	    Tcl_WrongNumArgs(ip, 2, objv, "typename updateproc setfromanyproc");
	    return TCL_ERROR;
	 }
	 Tcl_HashEntry *res = Tcl_FindHashEntry(&TypeTable, (void *) objv[2]);
	 if (NULL != res) {
	    Tcl_AppendResult(ip, "Type ", Tcl_GetString(objv[2]), " already exists", NULL);
	    return TCL_ERROR;
	 }
	 res = Tcl_CreateHashEntry(&TypeTable, (void *) objv[2], &isnew);
	 Tcl_SetHashValue(res, NewScrobjType(ip,objv[2],objv[3],objv[4]));
	 return TCL_OK;
      }
   case CONVERT:
      {
	 if (objc != 4) {
	    Tcl_WrongNumArgs(ip, 2, objv, "typename value");
	    return TCL_ERROR;
	 }

	 if (TCL_OK != Tcl_ConvertToType(ip,objv[2],(Tcl_ObjType *) &ScrobjTypeName)) {
	    return TCL_ERROR;
	 }

	 ScrObjType *tp = (ScrObjType *) objv[2]->internalRep.twoPtrValue.ptr1;

	 if(objv[3]->typePtr != (Tcl_ObjType *) tp) {
	    if (TCL_OK != ScrobjSetproc(tp, ip, objv[3])) {
	       return TCL_ERROR;
	    }
	 }

	 Tcl_SetObjResult(ip, objv[3]->internalRep.twoPtrValue.ptr1);
	 return TCL_OK;
      }
   case VALUE:
      {
	 if (objc != 4) {
	    Tcl_WrongNumArgs(ip, 2, objv, "typename internal-rep");
	    return TCL_ERROR;
	 }

	 if (TCL_OK != Tcl_ConvertToType(ip,objv[2],(Tcl_ObjType *) &ScrobjTypeName)) {
	    return TCL_ERROR;
	 }

	 ScrObjType *tp = (ScrObjType *) INTDATAPTR(objv[2]);

	 Tcl_SetObjResult(ip, NewScrobj(tp,objv[3]));
	 return TCL_OK;
      }
   }
   return TCL_ERROR;
}

int Scrobjy_Init(Tcl_Interp *ip) {

   Tcl_InitStubs(ip, "8.0", 0);

   Tcl_InitObjHashTable(&TypeTable);
   Tcl_CreateObjCommand(ip, "scrobjy", ScrobjyCmd, (ClientData) 0, NULL);

   return TCL_OK;
}

