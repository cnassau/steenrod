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

Tcl_Obj *Tcl_NewVectorObj(vectorType *vt, void *dat) {
    Tcl_Obj *res = Tcl_NewObj();
    res->typePtr = &tclVector;
    PTR1(res) = vt;
    PTR2(res) = dat;
    Tcl_InvalidateStringRep(res);
    return res;
}

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
    dupPtr->typePtr = srcPtr->typePtr;
    PTR1(dupPtr) = vt;
    PTR2(dupPtr) = (vt->createCopy)(PTR2(srcPtr));
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
    dupPtr->typePtr = srcPtr->typePtr;
    PTR1(dupPtr) = vt;
    PTR2(dupPtr) = (vt->createCopy)(PTR2(srcPtr));
}

/**** utilities */

int Tcl_MatrixGetDimensions(Tcl_Interp *ip, Tcl_Obj *obj, int *cols, int *rows) {
    matrixType *mt = PTR1(obj);
    if (TCL_OK != Tcl_ConvertToMatrix(ip, obj)) return TCL_ERROR;
    (mt->getDimensions)(PTR2(obj), cols, rows);
    return TCL_OK;
}

/**** wrappers for the adlin routines  */

Tcl_Obj *Tcl_LiftCmd(primeInfo *pi, Tcl_Obj *inp, Tcl_Obj *lft, 
                     Tcl_Interp *ip, const char *progvar, int pmsk) {
    matrixType *mt = PTR1(inp), *mt2 = PTR1(lft);
    int krows, kcols, irows, icols;
    void *rdat;
    progressInfo pro;

    /* since we cannibalize lft, it must not be shared */
    if (Tcl_IsShared(lft))
        assert(NULL == "lft must not be shared in Tcl_OrthoCmd!");

    if ((NULL == (mt->liftFunc)) || (mt != mt2)) 
        assert(NULL == "lift computation not fully implemented");

    /* Note: a matrix with 0 rows does not have a well defined number 
     * of columns, since the string representation is always {}. So
     * we must only complain if (krows != 0). */

    Tcl_MatrixGetDimensions(ip, lft, &krows, &kcols);
    Tcl_MatrixGetDimensions(ip, inp, &irows, &icols);
    if (krows == 0) kcols = icols;
    if (kcols != icols) {
        Tcl_SetResult(ip, "inconsistent dimensions", TCL_STATIC);
        return NULL;
    }

    pro.ip = ip;
    pro.progvar = progvar;
    pro.pmsk = pmsk;

    Tcl_InvalidateStringRep(lft);
    rdat = (mt->liftFunc)(pi, PTR2(inp), PTR2(lft), (NULL != progvar) ? &pro : NULL);

    if (NULL == rdat) return NULL;

    return Tcl_NewMatrixObj(mt, rdat);
}

int Tcl_QuotCmd(primeInfo *pi, Tcl_Obj *ker, Tcl_Obj *inp, 
                     Tcl_Interp *ip, const char *progvar, int pmsk) {
    matrixType *mt = PTR1(inp), *mt2 = PTR1(ker);
    int krows, kcols, irows, icols;
    progressInfo pro;

    /* since we cannibalize ker, it must not be shared */
    if (Tcl_IsShared(ker))
        assert(NULL == "ker must not be shared in Tcl_QuotCmd!");

    if ((NULL == (mt->quotFunc)) || (mt != mt2)) 
        assert(NULL == "quotient computation not fully implemented");

    /* Note: a matrix with 0 rows does not have a well defined number 
     * of columns, since the string representation is always {}. So
     * we must only complain if (irows != 0). */

    Tcl_MatrixGetDimensions(ip, ker, &krows, &kcols);
    Tcl_MatrixGetDimensions(ip, inp, &irows, &icols);
    if (irows == 0) icols = kcols;
    if (kcols != icols) {
        Tcl_SetResult(ip, "inconsistent dimensions", TCL_STATIC);
        return TCL_ERROR;
    }

    pro.ip = ip;
    pro.progvar = progvar;
    pro.pmsk = pmsk;

    if (ker==inp) assert(NULL == "quot not fully implemented");

    Tcl_InvalidateStringRep(ker);
    (mt->quotFunc)(pi, PTR2(ker), PTR2(inp), (NULL != progvar) ? &pro : NULL);

    return TCL_OK;
}

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

    /* shouldn't reduce be implicitly done in the orthofunc... ? */
    if (NULL != mt->reduce) (mt->reduce)(PTR2(inp), pi->prime);

    res = (mt->orthoFunc)(pi, PTR2(inp), (NULL != progvar) ? &pro : NULL);

    if (NULL == res) return NULL;

    Tcl_InvalidateStringRep(inp);

    return Tcl_NewMatrixObj(mt, res);
}

#undef RETERR
#define RETERR(msg) \
{ if (NULL!=ip) Tcl_SetResult(ip,msg,TCL_VOLATILE); return TCL_ERROR; }  

/* VAddCmd tries to do "(*obj1) += (*obj2) mod modval" */

int VAddCmd(Tcl_Interp *ip, Tcl_Obj *obj1, Tcl_Obj *obj2, int scale, int modval) {
    vectorType *vt1, *vt2;
    void *vdat1, *vdat2;
    int d1, d2;

    if (Tcl_IsShared(obj1)) 
        assert(NULL == "obj1 must not be shared in VAddCmd");

    vt1 = vectorTypeFromTclObj(obj1);
    vt2 = vectorTypeFromTclObj(obj2);

    vdat1 = vectorFromTclObj(obj1);
    vdat2 = vectorFromTclObj(obj2);
    
    if (vdat1 == vdat2) 
        PTR2(obj1) = vdat1 = vt1->createCopy(vdat1);

    d1 = vt1->getLength(vdat1);
    d2 = vt2->getLength(vdat2);

    if (d1 != d2) RETERR("dimensions don't match");

    Tcl_InvalidateStringRep(obj1);

    if (SUCCESS != LAVadd((vectorType **) &PTR1(obj1), &PTR2(obj1), 
                          vt2, vdat2, scale, modval)) 
        RETERR("could not add vectors (LAVadd failed)");

    return TCL_OK;
}

/* MAddCmd tries to do "(*obj1) += (*obj2) mod modval" */

int MAddCmd(Tcl_Interp *ip, Tcl_Obj *obj1, Tcl_Obj *obj2, int scale, int modval) {
    matrixType *mt1, *mt2;
    void *mdat1, *mdat2;
    int r1, r2, c1, c2;

    if (Tcl_IsShared(obj1)) 
        assert(NULL == "obj1 must not be shared in MAddCmd");

    mt1 = matrixTypeFromTclObj(obj1);
    mt2 = matrixTypeFromTclObj(obj2);

    mdat1 = matrixFromTclObj(obj1);
    mdat2 = matrixFromTclObj(obj2);

    if (mdat1 == mdat2) 
        PTR2(obj1) = mdat1 = mt1->createCopy(mdat1);

    mt1->getDimensions(mdat1, &r1, &c1);
    mt2->getDimensions(mdat2, &r2, &c2);
    
    if ((r1 != r2) || (c1 != c2)) RETERR("dimensions don't match");

    Tcl_InvalidateStringRep(obj1);

    if (SUCCESS != LAMadd((matrixType **) &PTR1(obj1), &PTR2(obj1), 
                          mt2, mdat2, scale, modval)) 
        RETERR("could not add matrices (LAMadd failed)");

    return TCL_OK;
}

int ExtractColsCmd(Tcl_Interp *ip, Tcl_Obj *mat, int *ind, int num) {
    matrixType *mt1, *mt2;
    void *mdat1, *mdat2;
    int i, j, ir, ic, or, oc; /* input/output number of ros/columns */

    mt1   = matrixTypeFromTclObj(mat);
    mdat1 = matrixFromTclObj(mat);

    mt1->getDimensions(mdat1, &ir, &ic);

    or = ir; oc = num;

    mt2   = mt1;
    mdat2 = mt2->createMatrix(or, oc);

    if (NULL == mdat2) RETERR("out of memory");

    for (i=0;i<num;i++) {
        int idx = ind[i];
        for (j=0;j<ir;j++) {
            int val; 
            mt1->getEntry(mdat1, j, idx, &val);
            mt2->setEntry(mdat2, j, i, val);
        }
    }

    Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt2, mdat2));
    return TCL_OK;
}

int ExtractRowsCmd(Tcl_Interp *ip, Tcl_Obj *mat, int *ind, int num) {
    matrixType *mt1, *mt2;
    void *mdat1, *mdat2;
    int i, j, ir, ic, or, oc; /* input/output number of ros/columns */

    mt1   = matrixTypeFromTclObj(mat);
    mdat1 = matrixFromTclObj(mat);

    mt1->getDimensions(mdat1, &ir, &ic);

    or = num; oc = ic;

    if (NULL != mt1->shrinkRows) {
        mt2   = mt1;
        mdat2 = mt1->createCopy(mdat1); 
        
        if (NULL == mdat2) RETERR("out of memory");

        mt1->shrinkRows(mdat2, ind, num);

        Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt2, mdat2));
        return TCL_OK;     
    }

    mt2   = mt1;
    mdat2 = mt2->createMatrix(or, oc);

    if (NULL == mdat2) RETERR("out of memory");
    
    for (i=0;i<num;i++) {
        int idx = ind[i];
        for (j=0;j<ic;j++) {
            int val; 
            mt1->getEntry(mdat1, idx, j, &val);
            mt2->setEntry(mdat2, i, j, val);
        }
    }

    Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt2, mdat2));
    return TCL_OK;     
}

/* ---------- */

typedef enum {
    LIN_INVSRP, LIN_INVMAT, 
    LIN_ISMAT, LIN_ISVEC, 
    LIN_ADDV, LIN_ADDM, LIN_EXTRACT,
    LIN_ORTHO, LIN_QUOT, LIN_LIFT, LIN_DIMS
} LinalgCmdCode;

#define ENSURERANGE(bot,a,top) \
if (((a)<(bot))||((a)>=(top))) RETERR("index out of range"); 

static CONST char *rcnames[] = { "rows", "cols", NULL };

int tLinCombiCmd(ClientData cd, Tcl_Interp *ip, 
          int objc, Tcl_Obj *CONST objv[]) {
    LinalgCmdCode cdi = (LinalgCmdCode) cd;
    primeInfo *pi;
    int r, c, index;
    Tcl_Obj *varp[4], *res;
    const char *progvar;
    int pmsk, modval;

    if (NULL == ip) return TCL_ERROR; 

    switch (cdi) {
        case LIN_ISMAT:
            ENSUREARGS1(TP_ANY);
            if (TCL_OK == Tcl_ConvertToMatrix(ip, objv[1]))
                r = 1;
            else
                r = 0;
            Tcl_SetObjResult(ip, Tcl_NewBooleanObj(r));
            return TCL_OK;

        case LIN_ISVEC:
            ENSUREARGS1(TP_ANY);
            if (TCL_OK == Tcl_ConvertToVector(ip, objv[1]))
                r = 1;
            else
                r = 0;
            Tcl_SetObjResult(ip, Tcl_NewBooleanObj(r));
            return TCL_OK;

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

        case LIN_EXTRACT:
            if (objc != 4) {
                Tcl_WrongNumArgs(ip, 1, objv, 
                                 "rows/cols <matrix> <list of indices>");
                return TCL_ERROR;
            }

            if (TCL_OK != Tcl_GetIndexFromObj(ip, objv[1], rcnames, 
                                              "option", 0, &index))
                return TCL_ERROR;
            
            if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[2]))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToIntList(ip, objv[3])) 
                return TCL_ERROR;

            return index 
                ? ExtractColsCmd(ip, objv[2], ILgetIntPtr(objv[3]), 
                                 ILgetLength(objv[3])) 
                : ExtractRowsCmd(ip, objv[2], ILgetIntPtr(objv[3]), 
                                 ILgetLength(objv[3]));

        case LIN_DIMS:
            ENSUREARGS1(TP_MATRIX);
            if (TCL_OK != Tcl_MatrixGetDimensions(ip, objv[1], &r, &c))
                return TCL_ERROR;
            varp[0] = Tcl_NewIntObj(r);
            varp[1] = Tcl_NewIntObj(c);
            Tcl_SetObjResult(ip, Tcl_NewListObj(2, varp));
            return TCL_OK;

        case LIN_ADDM:
            ENSUREARGS4(TP_MATRIX,TP_MATRIX,TP_OPTIONAL,TP_INT);

            modval = 0;
            if (objc > 3) 
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &modval))
                    return TCL_ERROR;

            res = objv[1];

            /* make private copy of res */
            Tcl_IncrRefCount(res);
            if (Tcl_IsShared(res)) {
                Tcl_DecrRefCount(res);
                res = Tcl_DuplicateObj(res);
                Tcl_IncrRefCount(res);
            }

            if (TCL_OK != MAddCmd(ip, res, objv[2], 1, modval)) {
                Tcl_DecrRefCount(res);
                return TCL_ERROR;
            }

            Tcl_SetObjResult(ip, res);
            return TCL_OK;

        case LIN_ADDV:
            ENSUREARGS4(TP_VECTOR,TP_VECTOR,TP_OPTIONAL,TP_INT);

            modval = 0;
            if (objc > 3) 
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &modval))
                    return TCL_ERROR;

            res = objv[1];

            /* make private copy of res */
            Tcl_IncrRefCount(res);
            if (Tcl_IsShared(res)) {
                Tcl_DecrRefCount(res);
                res = Tcl_DuplicateObj(res);
                Tcl_IncrRefCount(res);
            }

            if (TCL_OK != VAddCmd(ip, res, objv[2], 1, modval)) {
                Tcl_DecrRefCount(res);
                return TCL_ERROR;
            }

            Tcl_SetObjResult(ip, res);
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

            if (Tcl_IsShared(varp[1])) {
                Tcl_DecrRefCount(varp[1]);
                varp[1] = Tcl_DuplicateObj(varp[1]);
                Tcl_IncrRefCount(varp[1]);
            }

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
            
            /* should we assert here that both refcounts are 1 ... ? */
            // printObj("varp[1]", varp[1]);
            // printObj("varp[2]", varp[2]);
            
            return TCL_OK;

        case LIN_QUOT:
            ENSUREARGS6(TP_PRIME,TP_VARNAME,TP_VARNAME,TP_OPTIONAL,TP_VARNAME,TP_INT);
            if (TCL_OK != Tcl_GetPrimeInfo(ip,objv[1],&pi))
                return TCL_ERROR;

            /* get matrix from var1 */
            varp[1] = Tcl_ObjGetVar2(ip, objv[2], NULL, TCL_LEAVE_ERR_MSG);
            if (NULL == varp[1]) return TCL_ERROR;
            if (TCL_OK != Tcl_ConvertToMatrix(ip, varp[1])) 
                RETERR("var1 does not contain a valid matrix");

            /* get matrix from var2 */
            varp[2] = Tcl_ObjGetVar2(ip, objv[3], NULL, TCL_LEAVE_ERR_MSG);
            if (NULL == varp[2]) return TCL_ERROR;
            if (TCL_OK != Tcl_ConvertToMatrix(ip, varp[2])) 
                RETERR("var2 does not contain a valid matrix");

            progvar = NULL; pmsk = 0;
            if (objc >= 5) progvar = Tcl_GetString(objv[4]); 
            if (objc >= 6) 
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[5], &pmsk))
                    RETERR("internal error in LIN_QUOT");

            /* detach matrix from var1 */
            Tcl_IncrRefCount(varp[1]);
            if (NULL == Tcl_ObjSetVar2(ip, objv[2], NULL, 
                                       Tcl_NewObj(), TCL_LEAVE_ERR_MSG)) {
                Tcl_DecrRefCount(varp[1]);
                return TCL_ERROR;
            }

            if (Tcl_IsShared(varp[1])) {
                Tcl_DecrRefCount(varp[1]);
                varp[1] = Tcl_DuplicateObj(varp[1]);
                Tcl_IncrRefCount(varp[1]);
            }

            if (TCL_OK != Tcl_QuotCmd(pi, varp[1], varp[2], ip, progvar, pmsk)) {
                Tcl_DecrRefCount(varp[1]);
                return TCL_ERROR;
            }

            /* put result into var1 */
            if (NULL == Tcl_ObjSetVar2(ip, objv[2], NULL, 
                                      varp[1], TCL_LEAVE_ERR_MSG)) {
                Tcl_DecrRefCount(varp[1]);
                return TCL_ERROR;
            }
            Tcl_DecrRefCount(varp[1]);
            return TCL_OK;

        case LIN_LIFT:
            ENSUREARGS6(TP_PRIME,TP_VARNAME,TP_VARNAME,TP_OPTIONAL,TP_VARNAME,TP_INT);
            if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[1], &pi))
                return TCL_ERROR;

            /* get matrix from var1 */
            varp[1] = Tcl_ObjGetVar2(ip, objv[2], NULL, TCL_LEAVE_ERR_MSG);
            if (NULL == varp[1]) return TCL_ERROR;
            if (TCL_OK != Tcl_ConvertToMatrix(ip, varp[1])) 
                RETERR("var1 does not contain a valid matrix");

            /* get matrix from var2 */
            varp[1] = Tcl_ObjGetVar2(ip, objv[3], NULL, TCL_LEAVE_ERR_MSG);
            if (NULL == varp[2]) return TCL_ERROR;
            if (TCL_OK != Tcl_ConvertToMatrix(ip, varp[2])) 
                RETERR("var2 does not contain a valid matrix");

            progvar = NULL; pmsk = 0;
            if (objc >= 5) progvar = Tcl_GetString(objv[4]); 
            if (objc >= 6) 
                if (TCL_OK != Tcl_GetIntFromObj(ip, objv[5], &pmsk))
                    RETERR("internal error in LIN_ORTHO");

            /* detach matrix from var2 */
            Tcl_IncrRefCount(varp[2]);
            if (NULL == Tcl_ObjSetVar2(ip, objv[3], NULL, 
                                       Tcl_NewObj(), TCL_LEAVE_ERR_MSG)) {
                Tcl_DecrRefCount(varp[2]);
                return TCL_ERROR;
            }

            if (Tcl_IsShared(varp[2])) {
                Tcl_DecrRefCount(varp[2]);
                varp[2] = Tcl_DuplicateObj(varp[2]);
                Tcl_IncrRefCount(varp[2]);
            }

            varp[3] = Tcl_LiftCmd(pi, varp[1], varp[2], ip, progvar, pmsk);
            if (NULL == varp[3]) {
                Tcl_DecrRefCount(varp[2]);
                return TCL_ERROR;
            }

            /* put result into var2 */
            if (NULL == Tcl_ObjSetVar2(ip, objv[3], NULL, 
                                       varp[2], TCL_LEAVE_ERR_MSG)) {
                Tcl_DecrRefCount(varp[2]);
                return TCL_ERROR;
            }

            Tcl_DecrRefCount(varp[2]);
            Tcl_SetObjResult(ip, varp[3]);
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

#if 0
    CREATECOMMAND("invVct", LIN_INVSRP);
    CREATECOMMAND("invMat", LIN_INVMAT);
#endif

    CREATECOMMAND("ismatrix", LIN_ISMAT);
    CREATECOMMAND("isvector", LIN_ISVEC);

    CREATECOMMAND("addmatrix", LIN_ADDM);
    CREATECOMMAND("addvector", LIN_ADDV);

    CREATECOMMAND("extract", LIN_EXTRACT);

    CREATECOMMAND("getdims", LIN_DIMS);
    CREATECOMMAND("ortho",   LIN_ORTHO);
    CREATECOMMAND("lift",    LIN_LIFT);
    CREATECOMMAND("quot",    LIN_QUOT);

    Tcl_Eval(ip, "namespace eval " NSP " { namespace export * }");

    return TCL_OK;
}

