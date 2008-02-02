/*
 * Generic pointers for the Tcl interface 
 *
 * Copyright (C) 2004-2008 Christian Nassau <nassau@nullhomotopie.de>
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

#define PTR1(objptr) ((objptr)->internalRep.twoPtrValue.ptr1)
#define PTR2(objptr) ((objptr)->internalRep.twoPtrValue.ptr2)

/*::: Implementation of the IntList object type ::::::::::::::::::::::::::::::*/

/* An IntList represents a list of arbitrary precision integers. 
 * It uses the two-pointer value like this:
 * 
 *   ptr1 = ((length of the list) << 1) | (fits-to-int ? 0 : 1)
 *   ptr2 = (fits-to-int ? (pointer to int) : (pointer to extra struct) 
 *
 * Currently, only the ordinary precision case is implemented. 
 */

int  ILisXXL(Tcl_Obj *obj)     { return (USGNFROMVPTR(PTR1(obj))) & 1; }
int  ILgetLength(Tcl_Obj *obj) { return (USGNFROMVPTR(PTR1(obj))) >> 1; }
int *ILgetIntPtr(Tcl_Obj *obj) { return (int *) PTR2(obj); }

static Tcl_ObjType IntList;

int Tcl_ConvertToIntList(Tcl_Interp *ip, Tcl_Obj *obj) {
    return Tcl_ConvertToType(ip, obj, &IntList);
}

/* free internal representation */
void ILFreeInternalRepProc(Tcl_Obj *obj) {
    ckfree(PTR2(obj));
}

/* try to turn objPtr into a IntList */
int ILSetFromAnyProc(Tcl_Interp *ip, Tcl_Obj *objPtr) {
    int objc, i, *dat; 
    Tcl_Obj **objv;
    if (TCL_OK != Tcl_ListObjGetElements(ip, objPtr, &objc, &objv))
        return TCL_ERROR;
    if (NULL == (dat = (int *) ckalloc(sizeof(int) * objc))) 
        return TCL_ERROR;
    for (i=0;i<objc;i++)
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[i], &(dat[i]))) {
            ckfree((char *) dat); return TCL_ERROR;
        }
    /* Now we have a copy of the data. Free list representation. */
    objPtr->typePtr->freeIntRepProc(objPtr);
    PTR1(objPtr) = VPTRFROMUSGN(objc << 1);
    PTR2(objPtr) = dat;
    objPtr->typePtr = &IntList;
    return TCL_OK;
}

/* Create a new list Obj from the objPtr */
Tcl_Obj *ILToListObj(Tcl_Obj *objPtr) {
    int  len = ILgetLength(objPtr);
    int *dat = ILgetIntPtr(objPtr);
    Tcl_Obj *res, **arr; int i;
    arr = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * len);
    for (i=0;i<len;i++) 
        arr[i] = Tcl_NewIntObj(dat[i]);
    res = Tcl_NewListObj(len, arr);
    ckfree((char *) arr);
    return res;
}

/* recreate string representation */
void ILUpdateStringProc(Tcl_Obj *objPtr) {
    Tcl_Obj *lst = ILToListObj(objPtr);
    int slen; char *str = Tcl_GetStringFromObj(lst, &slen);
    objPtr->bytes = ckalloc(slen + 1);
    memcpy(objPtr->bytes, str, slen + 1);
    objPtr->length = slen;
}

/* create copy */
void ILDupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    int  len = ILgetLength(srcPtr);
    int *dat = ILgetIntPtr(srcPtr);
    int *ndat = (int *) ckalloc(sizeof(int) * len);
    memcpy(ndat,dat,sizeof(int) * len);
    PTR1(dupPtr) = VPTRFROMUSGN(len << 1);
    PTR2(dupPtr) = ndat;
}

/*::: Implementation of the TPtr object type :::::::::::::::::::::::::::::::::*/

Tcl_ObjType TPtr;

#define TCLRETERR(ip, msg) \
{ if (NULL!=ip) Tcl_SetResult(ip, msg, TCL_VOLATILE); return TCL_ERROR; }

#define TP_FORMAT "<%p:%p>"

void make_TPtr(Tcl_Obj *objPtr, int type, void *ptr) {
    objPtr->typePtr = &TPtr;
    objPtr->internalRep.twoPtrValue.ptr1 = VPTRFROMUSGN(type);
    objPtr->internalRep.twoPtrValue.ptr2 = ptr;
    Tcl_InvalidateStringRep(objPtr);
}

void *TPtr_GetPtr(Tcl_Obj *obj) { 
    return obj->internalRep.twoPtrValue.ptr2; 
} 

int   TPtr_GetType(Tcl_Obj *obj) { 
    return USGNFROMVPTR(obj->internalRep.twoPtrValue.ptr1); 
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
    make_TPtr(objPtr, USGNFROMVPTR(d1), d2); 
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
              USGNFROMVPTR(srcPtr->internalRep.twoPtrValue.ptr1), 
              srcPtr->internalRep.twoPtrValue.ptr2);
}

int TPtr_IsInitialized; 

int Tptr_Init(Tcl_Interp *ip) {

    Tcl_InitStubs(ip, "8.0", 0) ;

    if (TPtr_IsInitialized) return TCL_OK;
    TPtr_IsInitialized = 1;

    /* set up types and register */
    TPtr.name                  = "typed pointer";
    TPtr.freeIntRepProc        = NULL;
    TPtr.dupIntRepProc         = TPtr_DupInternalRepProc;
    TPtr.updateStringProc      = TPtr_UpdateStringProc;
    TPtr.setFromAnyProc        = TPtr_SetFromAnyProc;
    Tcl_RegisterObjType(&TPtr);
    
    IntList.name               = "list of integers";
    IntList.freeIntRepProc     = ILFreeInternalRepProc;
    IntList.dupIntRepProc      = ILDupInternalRepProc;
    IntList.updateStringProc   = ILUpdateStringProc;
    IntList.setFromAnyProc     = ILSetFromAnyProc;
    Tcl_RegisterObjType(&IntList);
    TPtr_RegObjType(TP_IL, &IntList);

    TPtr_RegType(TP_ANY,      "unspecified");
    TPtr_RegType(TP_INT,      "integer");
    TPtr_RegType(TP_LIST,     "list");
    TPtr_RegType(TP_INTLIST,  "list of integers");
    TPtr_RegType(TP_STRING,   "string");
    TPtr_RegType(TP_VARNAME,  "varname");
    TPtr_RegType(TP_PROCNAME, "procname");
    TPtr_RegType(TP_SCRIPT,   "script");
    TPtr_RegType(TP_PTR,      "typed pointer");
    TPtr_RegType(TP_VARARGS,  "...");

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
    Tcl_ObjType *type; 
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

TPtrTypeInfo *createNewType(int type, const char *name) {
    TPtrTypeInfo *aux, **auxp = &(TPtr_TypeList);
    if (NULL != (aux = findType(type))) {
        printf("Fatal problem in TPtr_RegType:\n"
               "cannot register type %s with id %d: already assigned to %s\n", 
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
    return (*auxp);
}

void TPtr_RegType(int type, const char *name) {
    TPtrTypeInfo *aux = createNewType(type, name);
    aux->id = type;
    aux->type = NULL;
    aux->name = strdup(name);
    aux->next = NULL;
}

void TPtr_RegObjType(int type, Tcl_ObjType *obtp) {
    TPtrTypeInfo *aux = createNewType(type, obtp->name);
    aux->id = type;
    aux->type = obtp;
    aux->name = strdup(obtp->name);
    aux->next = NULL;
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
    const char *space = " ";
    int type; 
    int optional = 0;
    if (NULL == ip) return ;
    wrk += sprintf(wrk, "problem with argument #%d", pos);
    if (NULL != msg) wrk += snprintf(wrk, 300, " (%s)", msg);
    if (NULL != name) {
        wrk += sprintf(wrk, "\nusage: %s", name);
        while (TP_END != (type = va_arg(*ap, int))) {
            wrk += sprintf(wrk, space);
            if (TP_OPTIONAL == type) { 
                optional = 1; 
                wrk += sprintf(wrk, "?");
                space = "";
                continue;
            }
            printTypename(typename, type);
            wrk += sprintf(wrk, "%s", typename);
            space = " ";
        }
        if (optional) wrk += sprintf(wrk, "?");
    }
    *wrk = 0;
    Tcl_SetResult(ip, err, TCL_VOLATILE);
}

#define CHCKARGSERR(msg)                                   \
do { va_end(ap);                                           \
va_start(ap, objv);                                        \
ckArgsErr(ip, Tcl_GetString(*objvorig), &ap, pos, msg);    \
va_end(ap);                                                \
return TCL_ERROR; } while (0)

int TPtr_CheckArgs(Tcl_Interp *ip, int objc, Tcl_Obj * CONST objv[], ...) {
    va_list ap;
    int type, lasttype; 
    int pos = 0;
    int optional = 0;
    int aux;

    TPtrTypeInfo *tpi;

    Tcl_Obj * CONST *objvorig = objv; /* backup copy */
  
    /* skip program name */
    objc--; objv++;

    va_start(ap, objv);

    for (pos=1, lasttype=-76823; TP_END != (type = va_arg(ap, int)); 
         lasttype=type, objc--, objv++, pos++) {

        /* process control args */
        if (TP_VARARGS   == type) { va_end(ap); return TCL_OK; }
        if (TP_OPTIONAL  == type) { optional = 1; objc++; objv--; pos--; continue; }
        if (TP_MANDATORY == type) { optional = 0; objc++; objv--; pos--; continue; }

        if (!objc) { /* no more args available */
            if (optional) { va_end(ap); return TCL_OK; }
            CHCKARGSERR("too few arguments"); 
        }

        if (0) printf("argchk: obj (%p), refcnt %d\n",*objv,(*objv)->refCount); 
  
        /* check for type mismatch */

        switch (type) {
            case TP_PROCNAME:
            case TP_VARNAME:
            case TP_SCRIPT:
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
        
        tpi = findType(type);
        
        /* check if Tcl_ObjType given */
        if ((NULL != tpi) && (NULL != tpi->type)) {
            if (TCL_OK != Tcl_ConvertToType(ip, *objv, tpi->type)) {
                char *aux = strdup(Tcl_GetStringResult(ip));
                CHCKARGSERR(aux);
                freex(aux);
            }
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

void copyStringRep(Tcl_Obj *dest, Tcl_Obj *src) {
    int slen; char *str = Tcl_GetStringFromObj(src, &slen);
    dest->bytes = ckalloc(slen + 1);
    memcpy(dest->bytes, str, slen + 1);
    dest->length = slen;
}

void printObj(const char *vname, Tcl_Obj *obj) {
    printf("Tcl_Obj (%s) at %p:\n",(NULL != vname) ? vname : "no name given", obj);
    printf("  refCount=%d, bytes=%p, length=%d, type=%p (%s)\n", 
           obj->refCount, obj->bytes, obj->length, obj->typePtr,
           (NULL != obj->typePtr) ? (obj->typePtr->name) : "---");
}
