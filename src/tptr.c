/*
 * Generic pointers for the Tcl interface 
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
#include <stdlib.h>
#include <string.h>

/*::: Implementation of the TPtr object type :::::::::::::::::::::::::::::::::*/
#define TCLRETERR(ip, msg) \
{ if (NULL!=ip) Tcl_SetResult(ip, msg, TCL_VOLATILE); return TCL_ERROR; }

#define TP_FORMAT "<%p:%p>"

void make_TPtr(Tcl_Obj *objPtr, int type, void *ptr) {
    objPtr->typePtr = &TPtr;
    objPtr->internalRep.twoPtrValue.ptr1 = (void *) type;
    objPtr->internalRep.twoPtrValue.ptr2 = ptr;
    Tcl_InvalidateStringRep(objPtr);
}

void *TPtr_GetPtr(Tcl_Obj *obj) { 
    return obj->internalRep.twoPtrValue.ptr2; 
} 

int   TPtr_GetType(Tcl_Obj *obj) { 
    return (int) obj->internalRep.twoPtrValue.ptr1; 
} 

/* try to turn objPtr into a TPtr */
int TPtr_SetFromAnyProc(Tcl_Interp *interp, Tcl_Obj *objPtr) {
    char *aux = Tcl_GetStringFromObj(objPtr, NULL);
    void *d1, *d2;
    if (2 != sscanf(aux, TP_FORMAT, &d1, &d2)) 
    TCLRETERR(interp, 
           "not recognized as TPtr, "
           "proper format = \"" TP_FORMAT "\""); 
    if (NULL != objPtr->typePtr)
    if (NULL != objPtr->typePtr->freeIntRepProc) 
        objPtr->typePtr->freeIntRepProc(objPtr);
    make_TPtr(objPtr, (int) d1, d2); 
    return TCL_OK;
}

/* recreate string representation */
void TPtr_UpdateStringProc(Tcl_Obj *objPtr) {
    objPtr->bytes = ckalloc(50);
    sprintf(objPtr->bytes, TP_FORMAT, 
         objPtr->internalRep.twoPtrValue.ptr1, 
         objPtr->internalRep.twoPtrValue.ptr2);
    objPtr->length = strlen(objPtr->bytes);
}

/* create copy */
void TPtr_DupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    make_TPtr(dupPtr, 
           (int) srcPtr->internalRep.twoPtrValue.ptr1, 
           srcPtr->internalRep.twoPtrValue.ptr2);
}

int TPtr_IsInitialized; 

int Tptr_Init(Tcl_Interp *ip) {

    if (TPtr_IsInitialized) return TCL_OK;
    TPtr_IsInitialized = 1;

    Tcl_InitStubs(ip, "8.0", 0) ;

    /* set up type and register */
    TPtr.name                  = "typed pointer";
    TPtr.freeIntRepProc        = NULL;
    TPtr.dupIntRepProc         = TPtr_DupInternalRepProc;
    TPtr.updateStringProc      = TPtr_UpdateStringProc;
    TPtr.setFromAnyProc        = TPtr_SetFromAnyProc;
    Tcl_RegisterObjType(&TPtr);
    
    TPtr_RegType(TP_ANY,     "anything");
    TPtr_RegType(TP_INT,     "integer");
    TPtr_RegType(TP_LIST,    "list");
    TPtr_RegType(TP_INTLIST, "list of integers");
    TPtr_RegType(TP_STRING,  "string");
    TPtr_RegType(TP_PTR,     "typed pointer");
    TPtr_RegType(TP_VARARGS, "...");

    return TCL_OK;
}

Tcl_Obj *Tcl_NewTPtr(int type, void *ptr) {
    Tcl_Obj *res = Tcl_NewObj(); 
    make_TPtr(res, type, ptr);
    return res;
}

/*::: TPtr_CheckArgs and friends :::::::::::::::::::::::::::::::::::::::::::::*/

typedef struct TPtrTypeInfo {
    int id;
    char *name;
    struct TPtrTypeInfo *next;
} TPtrTypeInfo;

TPtrTypeInfo *TPtr_TypeList = NULL;

TPtrTypeInfo *findType(int id) {
    TPtrTypeInfo *aux = TPtr_TypeList;
    while (NULL != aux) {
    if (aux->id == id) return aux;
    aux = aux->next;
    }
    return NULL;
}

void TPtr_RegType(int type, const char *name) {
    TPtrTypeInfo *aux, **auxp = &(TPtr_TypeList);
    if (NULL != (aux = findType(type))) {
    printf("Cannot register type %s with id %d: already assigned to %s\n", 
           name, type, aux->name);
    while (NULL != findType(++type)) ;
    printf("Suggest type id %d for type %s\n", type, name);
    exit(1);
    }
    while (NULL != *auxp) 
    auxp = &((*auxp)->next);
    if (NULL==(*auxp=(TPtrTypeInfo *) ckalloc(sizeof(TPtrTypeInfo)))) {
    printf("Out of memory\n");
    exit(1);
    }
    (*auxp)->id = type;
    (*auxp)->name = strdup(name);
    (*auxp)->next = NULL;
}

void printTypename(char *buf, int type) {
    TPtrTypeInfo *aux = findType(type);
    if (NULL == aux) {
    sprintf(buf, "<pointer of type %d>", type);
    } else {
    sprintf(buf, "<%s>", aux->name);
    }
}

void ckArgsErr(Tcl_Interp *ip, char *name, va_list *ap, int pos, char *msg) {
    char err[500], *wrk = err;
    char typename[100];
    int type; 
    int optional = 0;
    if (NULL == ip) return ;
    if (NULL != msg) wrk += sprintf(wrk, "%s", msg);
    else wrk += sprintf(wrk, "problem with argument #%d", pos);
    if (NULL != name) wrk += sprintf(wrk, "\nusage: %s", name);
    while (TP_END != (type = va_arg(*ap, int))) {
    if (TP_OPTIONAL == type) { 
        optional = 1; 
        wrk += sprintf(wrk, " [ ");
    }
    printTypename(typename, type);
    wrk += sprintf(wrk, " %s", typename);
    }
    if (optional) wrk += sprintf(wrk, " ]");
    Tcl_SetResult(ip, err, TCL_VOLATILE);
}

#define CHCKARGSERR(msg)                                   \
do { va_end(ap);                                           \
va_start(ap, objv);                                        \
ckArgsErr(ip, Tcl_GetString(*objvorig), &ap, pos, msg);  \
va_end(ap);                                                \
return TCL_ERROR; } while (0)

int TPtr_CheckArgs(Tcl_Interp *ip, int objc, Tcl_Obj * CONST objv[], ...) {
    va_list ap;
    int type; 
    int pos = 0;
    int optional = 0;
    int aux;
  
    Tcl_Obj * CONST *objvorig = objv; /* backup copy */
  
    /* skip program name */
    objc--; objv++;

    va_start(ap, objv);

    for (pos=1; TP_END != (type = va_arg(ap, int)); objc--, objv++, pos++) {
    /* process control args */
    if (TP_VARARGS   == type) { va_end(ap); return TCL_OK; }
    if (TP_OPTIONAL  == type) { optional = 1; continue; }
    if (TP_MANDATORY == type) { optional = 0; continue; }

    if (!objc) { /* no more args available */
        if (optional) { va_end(ap); return TCL_OK; }
        CHCKARGSERR("too few arguments"); 
    }
  
    /* check for type mismatch */

    switch (type) {
        case TP_ANY: 
        case TP_STRING:
        continue;
        case TP_INT:
        if (TCL_OK != Tcl_GetIntFromObj(ip, *objv, &aux))
            CHCKARGSERR(NULL); 
        continue;
        case TP_LIST:
        case TP_INTLIST:
        if (TCL_OK != Tcl_ListObjLength(ip, *objv, &aux))
            CHCKARGSERR(NULL); 
        if (TP_LIST == type) continue;
        {
            int obc, i; Tcl_Obj **obv;
            /* check list members:*/
            Tcl_ListObjGetElements(ip, *objv, &obc, &obv);
            for (i=0;i<obc;i++)
                if (TCL_OK != Tcl_GetIntFromObj(ip, obv[i], &aux))
                    CHCKARGSERR(NULL);     
        }
        continue;
        case TP_PTR:
        if (TCL_OK != Tcl_ConvertToType(ip, *objv, &TPtr)) 
            CHCKARGSERR(NULL); 
        continue;
    }    
    
    /* default : expect TPtr of given type */
    
    if (TCL_OK != Tcl_ConvertToType(ip, *objv, &TPtr)) 
        CHCKARGSERR(NULL); 

    if (TPtr_GetType(*objv) != type) 
        CHCKARGSERR(NULL); 

    /* Ok. Goto next arg */
    }

    if (objc) /* have some args left...? */
    CHCKARGSERR("too many arguments"); 

    va_end(ap);

    return TCL_OK;
}

/*::: Helper functions :::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

Tcl_Obj *Tcl_ListFromArray(int len, int *list) {
    Tcl_Obj *res, **array;
    int i;
    array = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * len);
    for (i=0;i<len;i++) array[i] = Tcl_NewIntObj(list[i]);
    res = Tcl_NewListObj(len, array);
    ckfree((char *) array);
    return res;
}

