/*
 * Tcl interface for the linear algebra routines
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

#include "tlin.h"
#include <string.h>

#define FREETCLOBJ(obj) { Tcl_IncrRefCount(obj); Tcl_DecrRefCount(obj); }

#define RETERR(errmsg) \
{ if (NULL != ip) Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; }

#define RETINT(i) { Tcl_SetObjResult(ip, Tcl_NewIntObj(i)); return TCL_OK; }

#define GETINT(ob, var)                                \
 if (TCL_OK != Tcl_GetIntFromObj(ip, ob, &privateInt)) \
    return TCL_ERROR;                                  \
 var = privateInt;

#define PTR1(objptr) ((objptr)->internalRep.twoPtrValue.ptr1)
#define PTR2(objptr) ((objptr)->internalRep.twoPtrValue.ptr2)

/**************************************************************************
 *
 * The Tcl object type for vectors represents them as a list of integers.
 *
 */

static Tcl_ObjType tclVector;

int Tcl_ConvertToVector(Tcl_Interp *ip, Tcl_Obj *obj) {
    return Tcl_ConvertToType(ip, obj, &tclVector);
}

int Tcl_ObjIsVector(Tcl_Obj *obj) { return &tclVector == obj->typePtr; }

void *vectorFromTclObj(Tcl_Obj *obj) { return PTR2(obj); }
vectorType *vectorTypeFromTclObj(Tcl_Obj *obj) { return (vectorType *) PTR1(obj); }

void VectorFreeInternalRepProc(Tcl_Obj *obj) {
    vectorType *vt = vectorTypeFromTclObj(obj);
    (vt->destroyVector)(vectorFromTclObj(obj));
}

int VectorSetFromAnyProc(Tcl_Interp *ip, Tcl_Obj *objPtr) {
    int objc, i, val;
    Tcl_Obj **objv;
    void *vct;

    if (TCL_OK != Tcl_ListObjGetElements(ip, objPtr, &objc, &objv))
        return TCL_ERROR;
    
    vct = (stdvector->createVector)(objc);

    for (i=0;i<objc;i++) {
        if (TCL_OK != Tcl_GetIntFromObj(ip,objv[i],&val)) { 
            (stdvector->destroyVector)(vct); 
            return TCL_ERROR; 
        }
        if (SUCCESS != (stdvector->setEntry)(vct,i,val)) {
            char msg[500];
            (stdvector->destroyVector)(vct);
            sprintf(msg, "value %d out of range", val);
            Tcl_SetResult(ip, msg, TCL_VOLATILE);
            return TCL_ERROR; 
        }
    }

    TRYFREEOLDREP(objPtr);
    PTR1(objPtr) = stdvector;
    PTR2(objPtr) = vct;
    objPtr->typePtr = &tclVector;

    return TCL_OK;
}

Tcl_Obj *Tcl_NewListFromVector(Tcl_Obj *obj) {
    int len, i;
    vectorType *vt = (vectorType *) PTR1(obj);
    Tcl_Obj **objv, *res;
    
    len = (vt->getLength)(PTR2(obj));
    
    if (NULL == (objv = mallox(len * sizeof(Tcl_Obj *)))) 
        return NULL;

    for (i=0;i<len;i++) {
        int val;
        if (SUCCESS != (vt->getEntry)(PTR2(obj), i, &val)) {
            freex(objv);
            return NULL;
        }
        objv[i] = Tcl_NewIntObj(val);
    }

    res = Tcl_NewListObj(len, objv);
    freex(objv);
    return res;
}

void VectorUpdateStringProc(Tcl_Obj *objPtr) {
    Tcl_Obj *aux = Tcl_NewListFromVector(objPtr);
    copyStringRep(objPtr, aux);
    FREETCLOBJ(aux);
}

void VectorDupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    vectorType *vt = PTR1(srcPtr);
    PTR1(dupPtr) = vt;
    PTR2(dupPtr) = (vt->createCopy)(PTR1(srcPtr));
}



/**************************************************************************
 *
 * The Tcl object type for matrices represents them as a list of list of integers.
 *
 */

static Tcl_ObjType tclMatrix;

int Tcl_ConvertToMatrix(Tcl_Interp *ip, Tcl_Obj *obj) {
    return Tcl_ConvertToType(ip, obj, &tclMatrix);
}

int Tcl_ObjIsMatrix(Tcl_Obj *obj) { return &tclMatrix == obj->typePtr; }

void *matrixFromTclObj(Tcl_Obj *obj) { return PTR2(obj); }
matrixType *matrixTypeFromTclObj(Tcl_Obj *obj) { return (matrixType *) PTR1(obj); }

Tcl_Obj *Tcl_NewMatrixObj(matrixType *mt, void *dat) {
    Tcl_Obj *res = Tcl_NewObj();
    res->typePtr = &tclMatrix;
    PTR1(res) = mt;
    PTR2(res) = dat;
    Tcl_InvalidateStringRep(res);
    return res;
}

void MatrixFreeInternalRepProc(Tcl_Obj *obj) {
    matrixType *vt = matrixTypeFromTclObj(obj);
    (vt->destroyMatrix)(matrixFromTclObj(obj));
}

#define FREEMATANDRETERR \
{ if (NULL != mat) (stdmatrix->destroyMatrix)(mat); return TCL_ERROR; }

int MatrixSetFromAnyProc(Tcl_Interp *ip, Tcl_Obj *objPtr) {
    int objc, objc2, rows, cols = 0, i, j, val;
    Tcl_Obj **objv, **objv2;
    void *mat = NULL;

    if (TCL_OK != Tcl_ListObjGetElements(ip, objPtr, &objc, &objv))
        return TCL_ERROR;
    
    rows = objc;

    for (i=0;i<objc;i++) {
        if (TCL_OK != Tcl_ListObjGetElements(ip, objv[i], &objc2, &objv2))
            FREEMATANDRETERR;
        if (NULL == mat) {
            cols = objc2;
            if (NULL == (mat = (stdmatrix->createMatrix)(rows,cols))) {
                Tcl_SetResult(ip, "out of memory", TCL_STATIC);
                FREEMATANDRETERR;
            }
        } else {
            if (cols != objc2) {
                Tcl_SetResult(ip, "vectors of different length in matrix", 
                              TCL_STATIC);
                FREEMATANDRETERR;
            }
        }
        for (j=0;j<objc2;j++) {
            if (TCL_OK != Tcl_GetIntFromObj(ip,objv2[j],&val)) 
                FREEMATANDRETERR;
             
            if (SUCCESS != (stdmatrix->setEntry)(mat,i,j,val)) {
                char msg[500];
                sprintf(msg, "value %d out of range", val);
                Tcl_SetResult(ip, msg, TCL_VOLATILE);
                FREEMATANDRETERR; 
            }
        }
    }

    if (NULL == mat) {
        assert(0 == rows);
        mat = (stdmatrix->createMatrix)(0,0);
    }
            
    TRYFREEOLDREP(objPtr);
    PTR1(objPtr) = stdmatrix;
    PTR2(objPtr) = mat;
    objPtr->typePtr = &tclMatrix;

    return TCL_OK;
}

Tcl_Obj *Tcl_NewListFromMatrix(Tcl_Obj *obj) {
    int rows, cols, i, j;
    matrixType *vt = (matrixType *) PTR1(obj);
    Tcl_Obj **objv, **objv2, *res;
    
    (vt->getDimensions)(PTR2(obj), &rows, &cols);
    
    if (NULL == (objv = mallox(rows * sizeof(Tcl_Obj *)))) 
        return NULL;

    if (NULL == (objv2 = mallox(cols * sizeof(Tcl_Obj *)))) {
        freex(objv);
        return NULL;
    }

    for (j=0;j<rows;j++) {
        for (i=0;i<cols;i++) {
            int val;
            if (SUCCESS != (vt->getEntry)(PTR2(obj), j, i, &val)) {
                freex(objv);
                return NULL;
            }
            objv2[i] = Tcl_NewIntObj(val);
        }

        objv[j] = Tcl_NewListObj(cols, objv2);
    }

    res = Tcl_NewListObj(rows, objv);
    freex(objv);
    freex(objv2);
    return res;
}

void MatrixUpdateStringProc(Tcl_Obj *objPtr) {
    Tcl_Obj *aux = Tcl_NewListFromMatrix(objPtr);
    copyStringRep(objPtr, aux);
    FREETCLOBJ(aux);
}

void MatrixDupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    matrixType *vt = PTR1(srcPtr);
    PTR1(dupPtr) = vt;
    PTR2(dupPtr) = (vt->createCopy)(PTR1(srcPtr));
}

/**** wrappers for the adlin routines  */

Tcl_Obj *Tcl_OrthoCmd(primeInfo *pi, Tcl_Obj *inp, 
                      Tcl_Interp *ip, const char *progvar, int pmsk) {
    matrixType *mt = PTR1(inp);
    progressInfo pro;
    void *res;
    /* since we cannibalize inp, it must not be shared */
    if (Tcl_IsShared(inp))
        assert(NULL == "inp must not be shared in Tcl_OrthoCmd!");
    if (NULL == (mt->orthoFunc)) {
        assert(NULL == "orthonormalization not fully implemented");
    }
    pro.ip = ip;
    pro.progvar = progvar;
    pro.pmsk = pmsk;
    if (NULL != mt->reduce) (mt->reduce)(PTR2(inp), pi->prime);
    res = (mt->orthoFunc)(pi, PTR2(inp), (NULL != progvar) ? &pro : NULL);
    if (NULL == res) return NULL;
    Tcl_InvalidateStringRep(inp);
    return Tcl_NewMatrixObj(mt, res);
}

#define NSP "linalg::"

typedef enum {
    LIN_INVSRP, LIN_INVMAT, LIN_ORTHO, LIN_QUOT, LIN_LIFT
} LinalgCmdCode;

#undef RETERR
#define RETERR(msg) \
{ if (NULL!=ip) Tcl_SetResult(ip,msg,TCL_VOLATILE); return TCL_ERROR; }  

#define ENSURERANGE(bot,a,top) \
if (((a)<(bot))||((a)>=(top))) RETERR("index out of range"); 

int tLinCombiCmd(ClientData cd, Tcl_Interp *ip, 
          int objc, Tcl_Obj *CONST objv[]) {
    LinalgCmdCode cdi = (LinalgCmdCode) cd;
    primeInfo *pi;
    Tcl_Obj *varp[3];
    const char *progvar;
    int pmsk;

    if (NULL==ip) return TCL_ERROR; 

    switch (cdi) {
        case LIN_INVSRP: 
            ENSUREARGS1(TP_VECTOR);
            Tcl_InvalidateStringRep(objv[1]);
            Tcl_SetObjResult(ip, objv[1]);
            return TCL_OK;
        case LIN_INVMAT: 
            ENSUREARGS1(TP_MATRIX);
            Tcl_InvalidateStringRep(objv[1]);
            Tcl_SetObjResult(ip, objv[1]);
            return TCL_OK;
        case LIN_ORTHO:
            ENSUREARGS6(TP_PRIME,TP_VARNAME,TP_VARNAME,TP_OPTIONAL,TP_VARNAME,TP_INT);
            if (TCL_OK != Tcl_GetPrimeInfo(ip,objv[1],&pi))
                return TCL_ERROR;
            /* get matrix from var1 */
            varp[1] = Tcl_ObjGetVar2(ip, objv[2], NULL, TCL_LEAVE_ERR_MSG);
            if (NULL == varp[1]) return TCL_ERROR;
            if (TCL_OK != Tcl_ConvertToMatrix(ip, varp[1])) 
                RETERR("var1 does not contain a valid matrix");
            /* reset var2 */
            if (NULL == Tcl_ObjSetVar2(ip, objv[3], NULL, 
                                       Tcl_NewObj(), TCL_LEAVE_ERR_MSG))
                return TCL_ERROR;
            progvar = NULL; pmsk = 0;
            if (objc >= 5) progvar = Tcl_GetString(objv[4]); 
            if (objc >= 6) 
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[5], &pmsk))
                    RETERR("internal error in LIN_ORTHO");
            /* detach matrix from var1 */
            Tcl_IncrRefCount(varp[1]);
            if (NULL == Tcl_ObjSetVar2(ip, objv[2], NULL, 
                                       Tcl_NewObj(), TCL_LEAVE_ERR_MSG)) {
                Tcl_DecrRefCount(varp[1]);
                return TCL_ERROR;
            }
            if (Tcl_IsShared(varp[1]))
                varp[1] = Tcl_DuplicateObj(varp[1]);
            varp[2] = Tcl_OrthoCmd(pi, varp[1], ip, progvar, pmsk);
            if (NULL == varp[2]) RETERR("orthonormalization failed");
            /* set variables and return */
            if (NULL == Tcl_ObjSetVar2(ip, objv[2], NULL, 
                                      varp[1], TCL_LEAVE_ERR_MSG)) {
                Tcl_DecrRefCount(varp[1]);
                return TCL_ERROR;
            }
            Tcl_DecrRefCount(varp[1]);
            if (NULL == Tcl_ObjSetVar2(ip, objv[3], NULL, 
                                      varp[2], TCL_LEAVE_ERR_MSG)) 
                return TCL_ERROR;
            return TCL_OK;
        case LIN_QUOT:
            ENSUREARGS6(TP_PRIME,TP_VARNAME,TP_VARNAME,TP_OPTIONAL,TP_VARNAME,TP_INT);
            return TCL_OK;
        case LIN_LIFT:
            ENSUREARGS6(TP_PRIME,TP_VARNAME,TP_VARNAME,TP_OPTIONAL,TP_VARNAME,TP_INT);
            return TCL_OK;
    }

    Tcl_SetResult(ip, "internal error in tLinCombiCmd", TCL_STATIC);

    return TCL_ERROR;
}

int Tlin_HaveType;

int Tlin_Init(Tcl_Interp *ip) {

    if (NULL == Tcl_InitStubs(ip, "8.0", 0)) return TCL_ERROR;

    Tprime_Init(ip);

#define CREATECOMMAND(name, code) \
Tcl_CreateObjCommand(ip,NSP name,tLinCombiCmd,(ClientData) code, NULL);

    if (!Tlin_HaveType) {
        Tptr_Init(ip);
        
        /* set up types and register */
        tclVector.name               = "vector";
        tclVector.freeIntRepProc     = VectorFreeInternalRepProc;
        tclVector.dupIntRepProc      = VectorDupInternalRepProc;
        tclVector.updateStringProc   = VectorUpdateStringProc;
        tclVector.setFromAnyProc     = VectorSetFromAnyProc;
        Tcl_RegisterObjType(&tclVector);
        TPtr_RegObjType(TP_VECTOR, &tclVector);
        
        /* set up types and register */
        tclMatrix.name               = "matrix";
        tclMatrix.freeIntRepProc     = MatrixFreeInternalRepProc;
        tclMatrix.dupIntRepProc      = MatrixDupInternalRepProc;
        tclMatrix.updateStringProc   = MatrixUpdateStringProc;
        tclMatrix.setFromAnyProc     = MatrixSetFromAnyProc;
        Tcl_RegisterObjType(&tclMatrix);
        TPtr_RegObjType(TP_MATRIX, &tclMatrix);

        Tlin_HaveType = 1;
    }

    /* basic commands */
    CREATECOMMAND("invVct", LIN_INVSRP);
    CREATECOMMAND("invMat", LIN_INVMAT);

    CREATECOMMAND("ortho", LIN_ORTHO);
    CREATECOMMAND("lift",  LIN_LIFT);
    CREATECOMMAND("quot",  LIN_QUOT);

    return TCL_OK;
}

