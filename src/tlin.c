/*
 * Tcl interface for the linear algebra routines
 *
 * Copyright (C) 2004-2018 Christian Nassau <nassau@nullhomotopie.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define TLIN_C_INCLUDES

#include "tlin.h"
#include "adlin.h"
#include "steenrod.h"
#include <string.h>

#if USEOPENCL
#include "linwrp.h"
#include "opencl.h"
#endif

#define FREETCLOBJ(obj)                                                        \
    {                                                                          \
        INCREFCNT(obj);                                                        \
        DECREFCNT(obj);                                                        \
    }

#define RETERR(errmsg)                                                         \
    {                                                                          \
        if (NULL != ip)                                                        \
            Tcl_SetResult(ip, errmsg, TCL_VOLATILE);                           \
        return TCL_ERROR;                                                      \
    }

#define RETINT(i)                                                              \
    {                                                                          \
        Tcl_SetObjResult(ip, Tcl_NewIntObj(i));                                \
        return TCL_OK;                                                         \
    }

#define GETINT(ob, var)                                                        \
    if (TCL_OK != Tcl_GetIntFromObj(ip, ob, &privateInt))                      \
        return TCL_ERROR;                                                      \
    var = privateInt;

#define PTR1(objptr) ((objptr)->internalRep.twoPtrValue.ptr1)
#define PTR2(objptr) ((objptr)->internalRep.twoPtrValue.ptr2)

/**************************************************************************
 *
 * The Tcl object type for vectors represents them as a list of integers.
 *
 */

static int vecCount;

#if 0
#define INCVECCNT                                                              \
    {                                                                          \
        fprintf(stderr, "vecCount = %d (%s, %d)\n", ++vecCount, __FILE__,      \
                __LINE__);                                                     \
    }
#define DECVECCNT                                                              \
    {                                                                          \
        fprintf(stderr, "vecCount = %d (%s, %d)\n", --vecCount, __FILE__,      \
                __LINE__);                                                     \
    }
#else
#define INCVECCNT                                                              \
    { ++vecCount; }
#define DECVECCNT                                                              \
    { --vecCount; }
#endif

static Tcl_ObjType tclVector;

int Tcl_ConvertToVector(Tcl_Interp *ip, Tcl_Obj *obj) {
    return Tcl_ConvertToType(ip, obj, &tclVector);
}

int Tcl_ObjIsVector(Tcl_Obj *obj) { return &tclVector == obj->typePtr; }

void *vectorFromTclObj(Tcl_Obj *obj) { return PTR2(obj); }
vectorType *vectorTypeFromTclObj(Tcl_Obj *obj) {
    return (vectorType *)PTR1(obj);
}

Tcl_Obj *Tcl_NewVectorObj(vectorType *vt, void *dat) {
    Tcl_Obj *res = Tcl_NewObj();
    res->typePtr = &tclVector;
    PTR1(res) = vt;
    PTR2(res) = dat;
    Tcl_InvalidateStringRep(res);
    INCVECCNT;
    return res;
}

void VectorFreeInternalRepProc(Tcl_Obj *obj) {
    vectorType *vt = vectorTypeFromTclObj(obj);
    (vt->destroyVector)(vectorFromTclObj(obj));
    DECVECCNT;
}

int VectorSetFromAnyProc(Tcl_Interp *ip, Tcl_Obj *objPtr) {
    int objc, i, val;
    Tcl_Obj **objv;
    void *vct;

    if (TCL_OK != Tcl_ListObjGetElements(ip, objPtr, &objc, &objv))
        return TCL_ERROR;

    vct = (stdvector->createVector)(objc);
    if (NULL == vct) {
        Tcl_SetResult(ip, "out of memory in VectorSetFromAnyProc", TCL_STATIC);
        return TCL_ERROR;
    }

    for (i = 0; i < objc; i++) {
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[i], &val)) {
            (stdvector->destroyVector)(vct);
            return TCL_ERROR;
        }
        if (SUCCESS != (stdvector->setEntry)(vct, i, val)) {
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
    INCVECCNT;

    return TCL_OK;
}

Tcl_Obj *Tcl_NewListFromVector(Tcl_Obj *obj) {
    int len, i;
    vectorType *vt = (vectorType *)PTR1(obj);
    Tcl_Obj **objv, *res;

    len = (vt->getLength)(PTR2(obj));

    if (NULL == (objv = mallox(len * sizeof(Tcl_Obj *))))
        return NULL;

    for (i = 0; i < len; i++) {
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
    INCVECCNT;
}

/**************************************************************************
 *
 * The Tcl object type for matrices represents them as a list of list of
 * integers.
 *
 */

static int matCount;

#if 0
#define INCMATCNT                                                              \
    {                                                                          \
        fprintf(stderr, "matCount = %d (%s, %d)\n", ++matCount, __FILE__,      \
                __LINE__);                                                     \
    }
#define DECMATCNT                                                              \
    {                                                                          \
        fprintf(stderr, "matCount = %d (%s, %d)\n", --matCount, __FILE__,      \
                __LINE__);                                                     \
    }
#else
#define INCMATCNT                                                              \
    { ++matCount; }
#define DECMATCNT                                                              \
    { --matCount; }
#endif

static Tcl_ObjType tclMatrix;

int Tcl_ConvertToMatrix(Tcl_Interp *ip, Tcl_Obj *obj) {
    return Tcl_ConvertToType(ip, obj, &tclMatrix);
}

int Tcl_ObjIsMatrix(Tcl_Obj *obj) { return &tclMatrix == obj->typePtr; }

void *matrixFromTclObj(Tcl_Obj *obj) { return PTR2(obj); }
matrixType *matrixTypeFromTclObj(Tcl_Obj *obj) {
    return (matrixType *)PTR1(obj);
}

Tcl_Obj *Tcl_NewMatrixObj(matrixType *mt, void *dat) {
    Tcl_Obj *res = Tcl_NewObj();
    res->typePtr = &tclMatrix;
    PTR1(res) = mt;
    PTR2(res) = dat;
    Tcl_InvalidateStringRep(res);
    INCMATCNT;
    return res;
}

void MatrixFreeInternalRepProc(Tcl_Obj *obj) {
    matrixType *vt = matrixTypeFromTclObj(obj);
    (vt->destroyMatrix)(matrixFromTclObj(obj));
    DECMATCNT;
}

#define FREEMATANDRETERR                                                       \
    {                                                                          \
        if (NULL != mat)                                                       \
            (stdmatrix->destroyMatrix)(mat);                                   \
        return TCL_ERROR;                                                      \
    }

int MatrixSetFromAnyProc(Tcl_Interp *ip, Tcl_Obj *objPtr) {
    int objc, objc2, rows, cols = 0, i, j, val;
    Tcl_Obj **objv, **objv2;
    void *mat = NULL;

    if (TCL_OK != Tcl_ListObjGetElements(ip, objPtr, &objc, &objv))
        return TCL_ERROR;

    rows = objc;

    for (i = 0; i < objc; i++) {
        if (TCL_OK != Tcl_ListObjGetElements(ip, objv[i], &objc2, &objv2))
            FREEMATANDRETERR;
        if (NULL == mat) {
            cols = objc2;
            if (NULL == (mat = (stdmatrix->createMatrix)(rows, cols))) {
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
        for (j = 0; j < objc2; j++) {
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv2[j], &val))
                FREEMATANDRETERR;

            if (SUCCESS != (stdmatrix->setEntry)(mat, i, j, val)) {
                char msg[500];
                sprintf(msg, "value %d out of range", val);
                Tcl_SetResult(ip, msg, TCL_VOLATILE);
                FREEMATANDRETERR;
            }
        }
    }

    if (NULL == mat) {
        ASSERT(0 == rows);
        mat = (stdmatrix->createMatrix)(0, 0);
    }

    TRYFREEOLDREP(objPtr);
    PTR1(objPtr) = stdmatrix;
    PTR2(objPtr) = mat;
    objPtr->typePtr = &tclMatrix;
    INCMATCNT;

    return TCL_OK;
}

Tcl_Obj *Tcl_NewListFromMatrix(Tcl_Obj *obj) {
    int rows, cols, i, j;
    matrixType *vt = (matrixType *)PTR1(obj);
    Tcl_Obj **objv, **objv2, *res;

    (vt->getDimensions)(PTR2(obj), &rows, &cols);

    if (NULL == (objv = mallox(rows * sizeof(Tcl_Obj *))))
        return NULL;

    if (NULL == (objv2 = mallox(cols * sizeof(Tcl_Obj *)))) {
        freex(objv);
        return NULL;
    }

    for (j = 0; j < rows; j++) {
        for (i = 0; i < cols; i++) {
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
    INCMATCNT;
}

/**** utilities */

int Tcl_MatrixGetDimensions(Tcl_Interp *ip, Tcl_Obj *obj, int *rows,
                            int *cols) {
    matrixType *mt;
    if (TCL_OK != Tcl_ConvertToMatrix(ip, obj))
        return TCL_ERROR;
    mt = PTR1(obj);
    (mt->getDimensions)(PTR2(obj), rows, cols);
    return TCL_OK;
}

/**** wrappers for the adlin routines  */

#define FAILASSERT(msg)                                                        \
    {                                                                          \
        Tcl_SetResult(ip, msg, TCL_VOLATILE);                                  \
        return NULL;                                                           \
    }

Tcl_Obj *Tcl_LiftCmd(primeInfo *pi, Tcl_Obj *inp, Tcl_Obj *lft, Tcl_Obj *bas,
                     Tcl_Interp *ip, const char *progvar, int pmsk,
                     int *interruptVar) {
    matrixType *mt = PTR1(inp), *mt2 = PTR1(lft);
    int krows, kcols, irows, icols;
    void *rdat;
    progressInfo pro;

    if (NULL == bas) {
        if (Tcl_IsShared(inp))
            FAILASSERT("inp must not be shared in Tcl_LiftCmd!");
    }

    /* since we cannibalize lft, it must not be shared */
    if (Tcl_IsShared(lft))
        FAILASSERT("lft must not be shared in Tcl_LiftCmd!");

    if ((NULL == (mt->liftFunc)))
        FAILASSERT("matrix type has no lift func");

    if ((mt != mt2)) {
        FAILASSERT("matrix types differ");
    }

    if (PTR2(inp) == PTR2(lft))
        FAILASSERT("lifting matrix through itself not yet supported");

    /* Note: a matrix with 0 rows does not have a well defined number
     * of columns, since the string representation is always {}. So
     * we must only complain if (krows != 0). */

    Tcl_MatrixGetDimensions(ip, lft, &krows, &kcols);
    Tcl_MatrixGetDimensions(ip, inp, &irows, &icols);
    if (krows == 0)
        kcols = icols;
    if (irows == 0)
        icols = kcols;
    if (kcols != icols) {
        Tcl_SetResult(ip, "inconsistent dimensions", TCL_STATIC);
        return NULL;
    }

    pro.ip = ip;
    pro.progvar = progvar;
    pro.pmsk = pmsk;
    pro.interruptVar = interruptVar;

    Tcl_InvalidateStringRep(lft);
    if (NULL == bas)
        Tcl_InvalidateStringRep(inp);
    rdat = (mt->liftFunc)(pi, PTR2(inp), PTR2(lft), bas ? PTR2(bas) : NULL,
                          (NULL != progvar) ? &pro : NULL);

    if (NULL == rdat)
        return NULL;

    return Tcl_NewMatrixObj(mt, rdat);
}

int Tcl_QuotCmd(primeInfo *pi, Tcl_Obj *ker, Tcl_Obj *inp, Tcl_Interp *ip,
                const char *progvar, int pmsk, int *interruptVar) {
    matrixType *mt = PTR1(inp), *mt2 = PTR1(ker);
    int krows, kcols, irows, icols;
    progressInfo pro;

    /* since we cannibalize ker, it must not be shared */
    if (Tcl_IsShared(ker))
        ASSERT(NULL == "ker must not be shared in Tcl_QuotCmd!");

    if (mt != mt2) {
        /* replace both by std copies */
        if (mt2 != stdmatrix) {
            void *newker = createStdMatrixCopy(mt2, PTR2(ker));
            mt2->destroyMatrix(PTR2(ker));
            PTR1(ker) = stdmatrix;
            PTR2(ker) = newker;
            mt2 = stdmatrix;
        }
        if (mt != stdmatrix) {
            void *newim = createStdMatrixCopy(mt, PTR2(inp));
            mt->destroyMatrix(PTR2(inp));
            PTR1(inp) = stdmatrix;
            PTR2(inp) = newim;
            mt = stdmatrix;
        }
    }

    if ((NULL == (mt->quotFunc)) || (mt != mt2))
        ASSERT(NULL == "quotient computation not fully implemented");

    /* Note: a matrix with 0 rows does not have a well defined number
     * of columns, since the string representation is always {}. So
     * we must only complain if (irows != 0). */

    Tcl_MatrixGetDimensions(ip, ker, &krows, &kcols);
    Tcl_MatrixGetDimensions(ip, inp, &irows, &icols);
    if (irows == 0) {
        /* quotient modulo empty matrix: nothing to do */
        return TCL_OK;
    }
    if (kcols != icols) {
        Tcl_SetResult(ip, "inconsistent dimensions", TCL_STATIC);
        return TCL_ERROR;
    }

    pro.ip = ip;
    pro.progvar = progvar;
    pro.pmsk = pmsk;
    pro.interruptVar = interruptVar;

    if (ker == inp)
        ASSERT(NULL == "quot not fully implemented");

    Tcl_InvalidateStringRep(ker);
    (mt->quotFunc)(pi, PTR2(ker), PTR2(inp), (NULL != progvar) ? &pro : NULL);

    return TCL_OK;
}

Tcl_Obj *Tcl_OrthoCmd(primeInfo *pi, Tcl_Obj *inp, Tcl_Obj **urb,
                      Tcl_Interp *ip, int wantkernel, const char *progvar,
                      int pmsk, int *interruptVar) {
    matrixType *mt = PTR1(inp);
    progressInfo pro;
    void *res, *oth, *oth2;

    /* since we cannibalize inp, it must not be shared */
    if (Tcl_IsShared(inp))
        ASSERT(NULL == "inp must not be shared in Tcl_OrthoCmd!");

    if (NULL == (mt->orthoFunc)) {
        ASSERT(NULL == "orthonormalization not fully implemented");
    }

    pro.ip = ip;
    pro.progvar = progvar;
    pro.pmsk = pmsk;
    pro.interruptVar = interruptVar;

    /* shouldn't reduce be implicitly done in the orthofunc... ? */
    if (NULL != mt->reduce)
        (mt->reduce)(PTR2(inp), pi->prime);

    if (NULL != urb) {
        oth2 = &oth;
    } else {
        oth2 = NULL;
    }

    res = (mt->orthoFunc)(pi, PTR2(inp), oth2, wantkernel,
                          (NULL != progvar) ? &pro : NULL);

    if (NULL != urb) {
        *urb = Tcl_NewMatrixObj(mt, oth);
    }

    Tcl_InvalidateStringRep(inp);

    if (NULL == res)
        return NULL;

    return Tcl_NewMatrixObj(mt, res);
}

#undef RETERR
#define RETERR(msg)                                                            \
    {                                                                          \
        if (NULL != ip)                                                        \
            Tcl_SetResult(ip, msg, TCL_VOLATILE);                              \
        return TCL_ERROR;                                                      \
    }

/* VAddCmd tries to do "(*obj1) += (*obj2) mod modval" */

int VAddCmd(Tcl_Interp *ip, Tcl_Obj *obj1, Tcl_Obj *obj2, int scale,
            int modval) {
    vectorType *vt1, *vt2, *aux;
    void *vdat1, *vdat2;
    int d1, d2;

    if (Tcl_IsShared(obj1))
        ASSERT(NULL == "obj1 must not be shared in VAddCmd");

    vt1 = vectorTypeFromTclObj(obj1);
    vt2 = vectorTypeFromTclObj(obj2);

    vdat1 = vectorFromTclObj(obj1);
    vdat2 = vectorFromTclObj(obj2);

    if (vdat1 == vdat2)
        PTR2(obj1) = vdat1 = vt1->createCopy(vdat1);

    d1 = vt1->getLength(vdat1);
    d2 = vt2->getLength(vdat2);

    if (d1 != d2)
        RETERR("dimensions don't match");

    Tcl_InvalidateStringRep(obj1);

    aux = (vectorType *)PTR1(obj1);
    if (SUCCESS != LAVadd(&aux, &PTR2(obj1), vt2, vdat2, scale, modval))
        RETERR("could not add vectors (LAVadd failed)");
    PTR1(obj1) = aux;

    return TCL_OK;
}

/* MAddCmd tries to do "(*obj1) += (*obj2) mod modval" */

int MAddCmd(Tcl_Interp *ip, Tcl_Obj *obj1, Tcl_Obj *obj2, int scale,
            int modval) {
    matrixType *mt1, *mt2, *aux;
    void *mdat1, *mdat2;
    int r1, r2, c1, c2;

    if (Tcl_IsShared(obj1))
        ASSERT(NULL == "obj1 must not be shared in MAddCmd");

    mt1 = matrixTypeFromTclObj(obj1);
    mt2 = matrixTypeFromTclObj(obj2);

    mdat1 = matrixFromTclObj(obj1);
    mdat2 = matrixFromTclObj(obj2);

    if (mdat1 == mdat2)
        PTR2(obj1) = mdat1 = mt1->createCopy(mdat1);

    mt1->getDimensions(mdat1, &r1, &c1);
    mt2->getDimensions(mdat2, &r2, &c2);

    if ((r1 != r2) || (c1 != c2))
        RETERR("dimensions don't match");

    Tcl_InvalidateStringRep(obj1);

    aux = (matrixType *)PTR1(obj1);
    if (SUCCESS != LAMadd(&aux, &PTR2(obj1), mt2, mdat2, scale, modval))
        RETERR("could not add matrices (LAMadd failed)");
    PTR1(obj1) = aux;

    return TCL_OK;
}

int ExtractColsCmd(Tcl_Interp *ip, Tcl_Obj *mat, int *ind, int num) {
    matrixType *mt1, *mt2;
    void *mdat1, *mdat2;
    int i, j, ir, ic, or, oc; /* input/output number of ros/columns */

    mt1 = matrixTypeFromTclObj(mat);
    mdat1 = matrixFromTclObj(mat);

    mt1->getDimensions(mdat1, &ir, &ic);

    or = ir;
    oc = num;

    mt2 = mt1;
    mdat2 = mt2->createMatrix(or, oc);

    if (NULL == mdat2)
        RETERR("out of memory");

    for (i = 0; i < num; i++) {
        int idx = ind[i];
        if (idx < 0) {
            Tcl_SetResult(ip, "negative index", TCL_STATIC);
            mt2->destroyMatrix(mdat2);
            return TCL_ERROR;
        } else if (idx >= ic && ir > 0) {
            Tcl_SetResult(ip, "index too big", TCL_STATIC);
            mt2->destroyMatrix(mdat2);
            return TCL_ERROR;
        }
        for (j = 0; j < ir; j++) {
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

    mt1 = matrixTypeFromTclObj(mat);
    mdat1 = matrixFromTclObj(mat);

    mt1->getDimensions(mdat1, &ir, &ic);

    or = num;
    oc = ic;

    if (NULL != mt1->shrinkRows) {
        mt2 = mt1;
        mdat2 = mt1->shrinkRows(mdat1, ind, num);

        if (NULL == mdat2)
            RETERR("out of memory");

        Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt2, mdat2));
        return TCL_OK;
    }

    mt2 = mt1;
    mdat2 = mt2->createMatrix(or, oc);

    if (NULL == mdat2)
        RETERR("out of memory");

    for (i = 0; i < num; i++) {
        int idx = ind[i];
        for (j = 0; j < ic; j++) {
            int val;
            mt1->getEntry(mdat1, idx, j, &val);
            mt2->setEntry(mdat2, i, j, val);
        }
    }

    Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt2, mdat2));
    return TCL_OK;
}

/* ---------- */

/* "TakeMatrixFromVar" treats its argument as the name of a variable which
 * is expected to contain a matrix object. This matrix object is read,
 * an unshared copy is made whose refcount is incremented, the variable is
 * cleared, and the matrix returned. If an error occurs a message is left
 * in the interpreter. */

Tcl_Obj *TakeMatrixFromVar(Tcl_Interp *ip, Tcl_Obj *varname) {
    Tcl_Obj *res;

    if (NULL == (res = Tcl_ObjGetVar2(ip, varname, NULL, TCL_LEAVE_ERR_MSG)))
        return NULL;

    if (TCL_OK != Tcl_ConvertToMatrix(ip, res)) {
        Tcl_SetObjResult(ip, varname);
        Tcl_AppendResult(ip, " does not contain a valid matrix", NULL);
        return NULL;
    }

    INCREFCNT(res);
    if (NULL ==
        Tcl_ObjSetVar2(ip, varname, NULL, Tcl_NewObj(), TCL_LEAVE_ERR_MSG)) {
        DECREFCNT(res);
        return NULL;
    }

    if (Tcl_IsShared(res)) {
#if 0
        char cmd [100];
        sprintf(cmd, "puts \"duplicating matrix at %p in %s (refCount=%d)\";",
                res, Tcl_GetString(varname), res->refCount);
        Tcl_Eval(ip, cmd);
#endif
        DECREFCNT(res);
        res = Tcl_DuplicateObj(res);
        INCREFCNT(res);
    }

    return res;
}

/* TakeVectorFromVar does the same for vectors. */

Tcl_Obj *TakeVectorFromVar(Tcl_Interp *ip, Tcl_Obj *varname) {
    Tcl_Obj *res;

    if (NULL == (res = Tcl_ObjGetVar2(ip, varname, NULL, TCL_LEAVE_ERR_MSG)))
        return NULL;

    if (TCL_OK != Tcl_ConvertToVector(ip, res)) {
        Tcl_SetObjResult(ip, varname);
        Tcl_AppendResult(ip, " does not contain a valid vector", NULL);
        return NULL;
    }

    INCREFCNT(res);
    if (NULL ==
        Tcl_ObjSetVar2(ip, varname, NULL, Tcl_NewObj(), TCL_LEAVE_ERR_MSG)) {
        DECREFCNT(res);
        return NULL;
    }

    if (Tcl_IsShared(res)) {
        DECREFCNT(res);
        res = Tcl_DuplicateObj(res);
        INCREFCNT(res);
    }

    return res;
}

/**** Encoding and decoding
 * ********************************************************/

typedef enum { ENCHEX } enctype;

static const char *encnames[] = {"hex", (char *)NULL};

/* static enctype encmap[] = { ENCHEX }; */

int Tcl_DecodeCmd(Tcl_Interp *ip, Tcl_Obj *in) {

    Tcl_Obj **objv;
    int objc, result, index;

    int nrows, ncols, blocksize, i, j, mask;

    matrixType *mt;
    void *mdat;

    if (TCL_OK != Tcl_ListObjGetElements(ip, in, &objc, &objv))
        return TCL_ERROR;

    if (objc != 5) {
        Tcl_SetResult(ip, "wrong number of entries in list", TCL_STATIC);
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(ip, objv[0], encnames, "encoding", 0, &index);
    if (result != TCL_OK)
        return result;

    if (TCL_OK != Tcl_GetIntFromObj(ip, objv[1], &blocksize))
        return TCL_ERROR;

    switch (blocksize) {
    case 1:
        mask = 0x01;
        break;
    case 2:
        mask = 0x03;
        break;
    case 4:
        mask = 0x0f;
        break;
    case 8:
        mask = 0xff;
        break;
    default:
        Tcl_SetResult(ip, "Unsupported blocksize: should be 1, 2, 4, or 8",
                      TCL_STATIC);
        return TCL_ERROR;
    }

    if (TCL_OK != Tcl_GetIntFromObj(ip, objv[2], &nrows))
        return TCL_ERROR;

    if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &ncols))
        return TCL_ERROR;

    if (TCL_OK != Tcl_ListObjLength(ip, objv[4], &index))
        return TCL_ERROR;

    if (ncols && (index != nrows)) {
        Tcl_SetResult(ip, "inconsistent number of rows", TCL_STATIC);
        return TCL_ERROR;
    }

    if (1 == blocksize) {
        mt = stdmatrix2;
    } else {
        mt = stdmatrix;
    }

    mdat = mt->createMatrix(nrows, ncols);

    if (NULL == mdat)
        RETERR("out of memory");

    if (0 == ncols) {
        Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt, mdat));
        return TCL_OK;
    }

    for (i = 0; i < nrows; i++) {
        Tcl_Obj *rowPtr;
        char *row;
        if (TCL_OK != Tcl_ListObjIndex(ip, objv[4], i, &rowPtr))
            ASSERT(NULL == "unexpected error in TclDecodeCmd (I)");
        if (NULL == rowPtr)
            ASSERT(NULL == "unexpected error in TclDecodeCmd (II)");
        row = Tcl_GetString(rowPtr);

        for (j = 0; j < ncols;) {
            unsigned char h, l, x, v;
            if (0 == (h = *row++))
                break;
            if (0 == (l = *row++))
                break;

#define FROMHEX(ch) (((ch) >= 'a') ? ((ch) - 'a' + 10) : ((ch) - '0'))

            x = FROMHEX(h);
            x <<= 4;
            x |= FROMHEX(l);

#define EXTRACT(sz)                                                            \
    {                                                                          \
        v = (x >> (8 - sz)) & mask;                                            \
        if (j >= ncols)                                                        \
            break;                                                             \
        mt->setEntry(mdat, i, j++, v);                                         \
        x <<= sz;                                                              \
    }

            switch (blocksize) {
            case 1:
                EXTRACT(1);
                EXTRACT(1);
                EXTRACT(1);
                EXTRACT(1);
                EXTRACT(1);
                EXTRACT(1);
                EXTRACT(1);
                EXTRACT(1);
                break;
            case 2:
                EXTRACT(2);
                EXTRACT(2);
                EXTRACT(2);
                EXTRACT(2);
                break;
            case 4:
                EXTRACT(4);
                EXTRACT(4);
                break;
            case 8:
                EXTRACT(8);
                break;
            }
        }
    }

    Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt, mdat));
    return TCL_OK;
}

int Tcl_Encode64Cmd(Tcl_Interp *ip, int base, Tcl_Obj *mat) {

    matrixType *mt;
    void *mdat;

    int blocksize = -1, nrows, ncols, len, i, j, val, rcs, mask;

    Tcl_Obj *res[5];
    char *enc, *wrk;

    /* For the moment we support only a very basic hexadecimal encoding:
     *
     * The format is {hex <blocksize> <num rows> <num cols> {col1 col2 ...}}.
     *
     * Each column is is encoded as a string of hexadecimal digits. Supported
     * blocksizes are 1, 2, 4, and 8.
     */

    if ((base > 1) && (base <= 2)) {
        blocksize = 1;
    } else if ((base > 2) && (base <= 4)) {
        blocksize = 2;
    } else if ((base > 4) && (base <= 16)) {
        blocksize = 4;
    } else if ((base > 16) && (base <= 256)) {
        blocksize = 8;
    }

    if (blocksize < 0) {
        Tcl_SetResult(ip, "base must be between 2 and 255", TCL_STATIC);
        return TCL_ERROR;
    }

    mask = (1 << blocksize) - 1;

    mt = matrixTypeFromTclObj(mat);
    mdat = matrixFromTclObj(mat);

    mt->getDimensions(mdat, &nrows, &ncols);

    /* Calculate and allocate necessary space */

    len = nrows; /* number of spaces as separators + one newline */
    rcs = (7 + ncols * blocksize) / 8; /* the number of bytes per row */
    rcs *= 2; /* now the number of hex digits per row */
    len += nrows * rcs;

    if (NULL == (enc = Tcl_AttemptAlloc(len + 1))) {
        Tcl_SetResult(ip, "out of memory", TCL_STATIC);
        return TCL_ERROR;
    }

    wrk = enc;

#define TOHEX(z) (((z) < 10) ? ('0' + (z)) : ('a' + (z)-10))

    for (i = 0; i < nrows; i++) {
        if (i)
            *wrk++ = ' ';

        for (j = 0; j < ncols;) {
            unsigned char c, l, h;
            c = 0;
            switch (blocksize) {

#define NEXT(sz)                                                               \
    {                                                                          \
        c <<= sz;                                                              \
        if (j >= ncols)                                                        \
            val = 0;                                                           \
        else                                                                   \
            mt->getEntry(mdat, i, j++, &val);                                  \
        c |= (val & mask);                                                     \
    }

            case 1:
                NEXT(1);
                NEXT(1);
                NEXT(1);
                NEXT(1);
                NEXT(1);
                NEXT(1);
                NEXT(1);
                NEXT(1);
                break;
            case 2:
                NEXT(2);
                NEXT(2);
                NEXT(2);
                NEXT(2);
                break;
            case 4:
                NEXT(4);
                NEXT(4);
                break;
            case 8:
                NEXT(8);
                break;
            }

            l = TOHEX((c & 0xf));
            c >>= 4;
            h = TOHEX((c & 0xf));

            *wrk++ = h;
            *wrk++ = l;
        }
    }

    *wrk++ = 0;

    res[0] = Tcl_NewStringObj("hex", 3);
    res[1] = Tcl_NewIntObj(blocksize);
    res[2] = Tcl_NewIntObj(nrows);
    res[3] = Tcl_NewIntObj(ncols);
    res[4] = Tcl_NewObj();
    res[4]->length = len - 1;
    res[4]->bytes = enc;

    Tcl_SetObjResult(ip, Tcl_NewListObj(5, res));
    return TCL_OK;
}

int Tcl_Convert2Cmd(Tcl_Interp *ip, Tcl_Obj *inmat) {
    void *res, *mdat;
    matrixType *mt;
    int i, j, rows, cols;

    if (TCL_OK != Tcl_ConvertToMatrix(ip, inmat))
        return TCL_ERROR;

    mt = matrixTypeFromTclObj(inmat);
    mdat = matrixFromTclObj(inmat);

    mt->getDimensions(mdat, &rows, &cols);

    res = stdmatrix2->createMatrix(rows, cols);

    if (NULL == res) {
        Tcl_SetResult(ip, "Out of memory", TCL_STATIC);
        return TCL_ERROR;
    }

    for (i = rows; i--;) {
        for (j = cols; j--;) {
            int val;
            mt->getEntry(mdat, i, j, &val);
            if ((val != 0) && (val != 1)) {
                char errmsg[80];
                sprintf(errmsg, "all entries must be 0 or 1 (found %d)", val);
                stdmatrix2->destroyMatrix(res);
                Tcl_SetResult(ip, errmsg, TCL_VOLATILE);
                return TCL_ERROR;
            }
            stdmatrix2->setEntry(res, i, j, val);
        }
    }

    Tcl_SetObjResult(ip, Tcl_NewMatrixObj(stdmatrix2, res));
    return TCL_OK;
}

int Tcl_MultMatrixCmd(Tcl_Interp *ip, primeInfo *pi, Tcl_Obj *f1, Tcl_Obj *f2) {

    matrixType *mt1, *mt2;
    void *mdat1, *mdat2, *mres;
    int rows, cols, inb, inb2, i, j, k, p = pi->prime;

    if (TCL_OK != Tcl_ConvertToMatrix(ip, f1))
        return TCL_ERROR;

    if (TCL_OK != Tcl_ConvertToMatrix(ip, f2))
        return TCL_ERROR;

    mt1 = matrixTypeFromTclObj(f1);
    mt2 = matrixTypeFromTclObj(f2);
    mdat1 = matrixFromTclObj(f1);
    mdat2 = matrixFromTclObj(f2);

    mt1->getDimensions(mdat1, &rows, &inb);
    mt2->getDimensions(mdat2, &inb2, &cols);

    if (inb != inb2) {
        Tcl_SetResult(ip, "dimensions don't match", TCL_STATIC);
        return TCL_ERROR;
    }

    mres = mt1->createMatrix(rows, cols);

    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            int aux = 0;
            for (k = 0; k < inb; k++) {
                int v, w;
                mt1->getEntry(mdat1, i, k, &v);
                mt2->getEntry(mdat2, k, j, &w);
                aux += v * w;
                aux %= p;
            }
            mt1->setEntry(mres, i, j, aux);
        }
    }

    Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt1, mres));
    return TCL_OK;
}

/**** Implementation of the matrix combi-command
 * ***********************************/

#if USEOPENCL
int MatrixCLCreatePostProc(ClientData data[], Tcl_Interp *ip, int result) {
    mat2 *m2 = (mat2 *)data[0];
    cl_mem clm = (cl_mem)data[1];
    stcl_context *ctx = (stcl_context *)data[2];
    int returnmatrix = (int)data[3];
    void *buf = NULL;
    if (returnmatrix && TCL_OK == result)
        do {
            result = TCL_ERROR;
            size_t dsz = m2->ipr * m2->rows * sizeof(int);
            m2->data = malloc(dsz ? dsz : 1);
            if (NULL == m2->data) {
                Tcl_SetResult(ip, "out of memory", TCL_STATIC);
                break;
            }
            int rc;
            cl_command_queue q = GetOrCreateCommandQueue(ip, ctx, 0);
            if (NULL == q)
                break;
            buf = clEnqueueMapBuffer(q, clm, 1 /* blocking */, CL_MAP_READ, 0,
                                     dsz, 0, NULL, NULL, &rc);
            if (NULL == buf || CL_SUCCESS != rc) {
                SetCLErrorCode(ip, rc);
                Tcl_AddErrorInfo(ip, " from clEnqueueMapBuffer");
                break;
            }
            memcpy(m2->data, buf, dsz);
            rc = clEnqueueUnmapMemObject(q, clm, buf, 0, NULL, NULL);
            if (CL_SUCCESS != rc) {
                SetCLErrorCode(ip, rc);
                Tcl_AddErrorInfo(ip, " from clEnqueueUnmapMemObject");
                break;
            }
            Tcl_SetObjResult(ip, Tcl_NewMatrixObj(stdmatrix2, m2));
            result = TCL_OK;
        } while (0);

    if (!returnmatrix || TCL_OK != result) {
        if (m2 && m2->data)
            free(m2->data);
        if (m2)
            free(m2);
    }
    clReleaseMemObject(clm);
    return result;
}

int MatrixCLMapPostProc(ClientData data[], Tcl_Interp *ip, int result) {
    Tcl_Obj *matobj = (Tcl_Obj *) data[0];
    Tcl_Obj *varname = (Tcl_Obj *) data[3];
    cl_mem clm = (cl_mem)data[1];
    stcl_context *ctx = (stcl_context *)data[2];
    if (TCL_OK == result)
        do {
            result = TCL_ERROR;
            mat2 *m2 = (mat2 *) matrixFromTclObj(matobj);
            size_t dsz = m2->ipr * m2->rows * sizeof(int);
            int rc;
            cl_command_queue q = GetOrCreateCommandQueue(ip, ctx, 0);
            if (NULL == q)
                break;
            rc = clEnqueueReadBuffer(q, clm, 1 /* blocking */, 0, dsz, m2->data,
                                     0, NULL, NULL);
            if (CL_SUCCESS != rc) {
                SetCLErrorCode(ip, rc);
                Tcl_AddErrorInfo(ip, " from clEnqueueReadBuffer");
                break;
            }
            result = TCL_OK;
        } while (0);
    clReleaseMemObject(clm);
    Tcl_ObjSetVar2(ip,varname,NULL,matobj,0);
    return result;
}
#endif

#define LINALG_INTERRUPT_VARIABLE (*interruptVar)

#define CHECK_INTERRUPT_VARIABLE(where)                                        \
    if (LINALG_INTERRUPT_VARIABLE) {                                           \
        Tcl_SetResult(ip, "computation has been interrupted", TCL_STATIC);     \
        return TCL_ERROR;                                                      \
    }

static const char *rcnames[] = {"rows", "cols", "single-row", "single-column",
                                NULL};

typedef enum {
    ORTHO,
    LIFT,
    LIFTV,
    QUOT,
    DIMS,
    CREATE,
    ADDTO,
    ISZERO,
    TEST,
    EXTRACT,
    ENCODE64,
    DECODE,
    TYPE,
    CONVERT2,
    MULT,
    UNIT,
    CONCAT,
    CLMAP,
    CLALLOC,
    CLCREATE,
    CLENQREAD
} matcmdcode;

static const char *mCmdNames[] = {
    "orthonormalize", "lift",   "liftvar", "quotient", "extract",
    "dimensions",     "create", "addto",   "iszero",   "test",
    "encode64",       "decode", "type",    "convert2", "multiply",
    "unit",           "concat", "clmap",   "clalloc", "clcreate", "clenqread", (char *)NULL};

static matcmdcode mCmdmap[] = {ORTHO,    LIFT,   LIFTV, QUOT,     EXTRACT,
                               DIMS,     CREATE, ADDTO, ISZERO,   TEST,
                               ENCODE64, DECODE, TYPE,  CONVERT2, MULT,
                               UNIT,     CONCAT, CLMAP, CLALLOC, CLCREATE, CLENQREAD};

int MatrixNRECombiCmd(ClientData cd, Tcl_Interp *ip, int objc,
                      Tcl_Obj *const objv[]) {
    int result, index, scale, modval, rows, cols;
    primeInfo *pi;
    matrixType *mt;
    void *mdat;
    Tcl_Obj *(varp[5]), *urbobj, **urb;
    int *interruptVar = (int *)cd;
    LINALG_INTERRUPT_VARIABLE = 0;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result =
        Tcl_GetIndexFromObj(ip, objv[1], mCmdNames, "subcommand", 0, &index);
    if (result != TCL_OK)
        return result;

#define EXPECTARGS(bas, min, max, msg)                                         \
    {                                                                          \
        if ((objc < ((bas) + (min))) || (objc > ((bas) + (max)))) {            \
            Tcl_WrongNumArgs(ip, (bas), objv, msg);                            \
            return TCL_ERROR;                                                  \
        }                                                                      \
    }

    switch (mCmdmap[index]) {
    case TYPE:
        EXPECTARGS(2, 1, 1, "<matrix>");

        if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[2]))
            return TCL_ERROR;

        Tcl_SetResult(ip, (char *)matrixTypeFromTclObj(objv[2])->name,
                      TCL_VOLATILE);
        return TCL_OK;

    case CONVERT2:
        EXPECTARGS(2, 1, 1, "<matrix>");

        return Tcl_Convert2Cmd(ip, objv[2]);

    case ENCODE64:
        EXPECTARGS(2, 2, 2, "<base> <matrix>");

        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[2], &scale))
            return TCL_ERROR;

        if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[3]))
            return TCL_ERROR;

        if (TCL_OK != Tcl_Encode64Cmd(ip, scale, objv[3])) {
            return TCL_ERROR;
        }

        return TCL_OK;

    case DECODE:
        EXPECTARGS(2, 1, 1, "<string>");

        if (TCL_OK != Tcl_DecodeCmd(ip, objv[2])) {
            return TCL_ERROR;
        }

        return TCL_OK;

    case QUOT:
        EXPECTARGS(2, 3, 3, "<prime> <varname> <matrix>");

        if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[2], &pi))
            return TCL_ERROR;

        if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[4]))
            return TCL_ERROR;

        if (NULL == (varp[1] = TakeMatrixFromVar(ip, objv[3])))
            return TCL_ERROR;

        if (TCL_OK != Tcl_QuotCmd(pi, varp[1], objv[4], ip, THEPROGVAR,
                                  theprogmsk, interruptVar)) {
            DECREFCNT(varp[1]);
            CHECK_INTERRUPT_VARIABLE(quot);
            return TCL_ERROR;
        }

        /* put result into var1 */
        if (NULL ==
            Tcl_ObjSetVar2(ip, objv[3], NULL, varp[1], TCL_LEAVE_ERR_MSG)) {
            DECREFCNT(varp[1]);
            return TCL_ERROR;
        }

        DECREFCNT(varp[1]);
        return TCL_OK;

    case LIFT:
        EXPECTARGS(2, 4, 4, "<prime> <matrix> <basis> <varname>");

        if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[2], &pi))
            return TCL_ERROR;

        if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[3]))
            return TCL_ERROR;

        if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[4]))
            return TCL_ERROR;

        if (NULL == (varp[2] = TakeMatrixFromVar(ip, objv[5])))
            return TCL_ERROR;

        varp[3] = Tcl_LiftCmd(pi, objv[3], varp[2], objv[4], ip, THEPROGVAR,
                              theprogmsk, interruptVar);

        if (NULL == varp[3]) {
            DECREFCNT(varp[2]);
            CHECK_INTERRUPT_VARIABLE(lift);
            return TCL_ERROR;
        }

        if (NULL ==
            Tcl_ObjSetVar2(ip, objv[5], NULL, varp[2], TCL_LEAVE_ERR_MSG)) {
            DECREFCNT(varp[2]);
            return TCL_ERROR;
        }

        DECREFCNT(varp[2]);

        Tcl_SetObjResult(ip, varp[3]);
        return TCL_OK;

    case LIFTV:
        EXPECTARGS(2, 3, 3, "<prime> <matrixvar> <toliftvar>");

        if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[2], &pi))
            return TCL_ERROR;

        if (NULL == (varp[4] = TakeMatrixFromVar(ip, objv[3])))
            return TCL_ERROR;

        if (NULL == (varp[2] = TakeMatrixFromVar(ip, objv[4])))
            return TCL_ERROR;

        varp[3] = Tcl_LiftCmd(pi, varp[4], varp[2], NULL, ip, THEPROGVAR,
                              theprogmsk, interruptVar);

        if (NULL == varp[3]) {
            DECREFCNT(varp[4]);
            DECREFCNT(varp[2]);
            CHECK_INTERRUPT_VARIABLE(lift);
            return TCL_ERROR;
        }

        if (NULL ==
            Tcl_ObjSetVar2(ip, objv[4], NULL, varp[2], TCL_LEAVE_ERR_MSG)) {
            DECREFCNT(varp[4]);
            DECREFCNT(varp[2]);
            return TCL_ERROR;
        }

        DECREFCNT(varp[4]);
        DECREFCNT(varp[2]);

        Tcl_SetObjResult(ip, varp[3]);
        return TCL_OK;

    case ORTHO: {
        int wantkernel;
        EXPECTARGS(2, 3, 4, "<prime> <inputvar> <kernelvar> ?<basisvar>?");

        if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[2], &pi))
            return TCL_ERROR;

        if (NULL == (varp[1] = TakeMatrixFromVar(ip, objv[3])))
            return TCL_ERROR;

        if (objc > 5) {
            urb = &urbobj;
        } else {
            urb = NULL;
        }

        wantkernel = (0 != *Tcl_GetString(objv[4]));

        varp[2] = Tcl_OrthoCmd(pi, varp[1], urb, ip, wantkernel, THEPROGVAR,
                               theprogmsk, interruptVar);

        if (wantkernel && NULL == varp[2]) {
            DECREFCNT(varp[1]);
            CHECK_INTERRUPT_VARIABLE(ortho);
            RETERR("orthonormalization failed");
        }

        /* set variables and return */
        if (NULL ==
            Tcl_ObjSetVar2(ip, objv[3], NULL, varp[1], TCL_LEAVE_ERR_MSG)) {
            DECREFCNT(varp[1]);
            DECREFCNT(varp[2]);
            return TCL_ERROR;
        }
        DECREFCNT(varp[1]);

        if (wantkernel && (NULL == Tcl_ObjSetVar2(ip, objv[4], NULL, varp[2],
                                                  TCL_LEAVE_ERR_MSG))) {
            DECREFCNT(varp[2]);
            return TCL_ERROR;
        }

        if ((NULL != urb) && (NULL == Tcl_ObjSetVar2(ip, objv[5], NULL, urbobj,
                                                     TCL_LEAVE_ERR_MSG))) {
            return TCL_ERROR;
        }

        return TCL_OK;
    }
    case EXTRACT:
        EXPECTARGS(2, 3, 3, "rows/cols <matrix> <list of indices>");

        if (TCL_OK !=
            Tcl_GetIndexFromObj(ip, objv[2], rcnames, "option", 0, &index))
            return TCL_ERROR;

        if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[3]))
            return TCL_ERROR;

        if (index < 2) {

            if (TCL_OK != Tcl_ConvertToIntList(ip, objv[4]))
                return TCL_ERROR;

            return index ? ExtractColsCmd(ip, objv[3], ILgetIntPtr(objv[4]),
                                          ILgetLength(objv[4]))
                         : ExtractRowsCmd(ip, objv[3], ILgetIntPtr(objv[4]),
                                          ILgetLength(objv[4]));
        } else {
            /* single row/column variant */

            int nrc;
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &nrc))
                return TCL_ERROR;

            void *vdat =
                CreateMatrixRowcolVector(objv[3], nrc, (index == 2) ? 1 : 0);
            Tcl_SetObjResult(ip, Tcl_NewVectorObj(&matrixRowcolVector, vdat));
            return TCL_OK;
        }
    case MULT:
        EXPECTARGS(2, 3, 3, "<prime> <matrix1> <matrix2>");

        if (TCL_OK != Tcl_GetPrimeInfo(ip, objv[2], &pi))
            return TCL_ERROR;

        return Tcl_MultMatrixCmd(ip, pi, objv[3], objv[4]);

    case DIMS:
        EXPECTARGS(2, 1, 1, "<matrix>");

        if (TCL_OK != Tcl_MatrixGetDimensions(ip, objv[2], &rows, &cols))
            return TCL_ERROR;

        varp[0] = Tcl_NewIntObj(rows);
        varp[1] = Tcl_NewIntObj(cols);

        Tcl_SetObjResult(ip, Tcl_NewListObj(2, varp));
        return TCL_OK;

    case CREATE:
        EXPECTARGS(2, 2, 2, "<rows> <columns>");

        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[2], &rows))
            return TCL_ERROR;

        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &cols))
            return TCL_ERROR;

        mt = stdmatrix;
        mdat = mt->createMatrix(rows, cols);

        if (NULL == mdat)
            RETERR("out of memory");

        Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt, mdat));
        return TCL_OK;

    case UNIT:
        EXPECTARGS(2, 1, 1, "<rows>");

        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[2], &rows))
            return TCL_ERROR;

        mt = stdmatrix;
        mdat = mt->createMatrix(rows, rows);

        if (NULL == mdat)
            RETERR("out of memory");

        for (cols = 0; cols < rows; cols++) {
            mt->setEntry(mdat, cols, cols, 1);
        }

        Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt, mdat));
        return TCL_OK;

    case ADDTO:
        EXPECTARGS(2, 2, 4, "<varname> <matrix> ?<scale>? ?<mod>?");

        scale = 1;
        modval = 0;

        if (objc > 4)
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &scale))
                return TCL_ERROR;

        if (objc > 5)
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[5], &modval))
                return TCL_ERROR;

        if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[3]))
            return TCL_ERROR;

        if (NULL == (varp[1] = TakeMatrixFromVar(ip, objv[2])))
            return TCL_ERROR;

        if (TCL_OK != MAddCmd(ip, varp[1], objv[3], scale, modval)) {
            DECREFCNT(varp[1]);
            return TCL_ERROR;
        }

        if (NULL ==
            Tcl_ObjSetVar2(ip, objv[2], NULL, varp[1], TCL_LEAVE_ERR_MSG)) {
            DECREFCNT(varp[1]);
            return TCL_ERROR;
        }
        DECREFCNT(varp[1]);

        Tcl_ResetResult(ip);
        return TCL_OK;

    case ISZERO:
        EXPECTARGS(2, 1, 1, "<matrix>");

        if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[2]))
            return TCL_ERROR;

        mt = matrixTypeFromTclObj(objv[2]);
        mdat = matrixFromTclObj(objv[2]);

        Tcl_SetObjResult(ip, Tcl_NewBooleanObj(matrixIsZero(mt, mdat)));
        return TCL_OK;

    case TEST:
        EXPECTARGS(2, 1, 1, "<matrix candidate>");

        if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[2]))
            return TCL_OK;

        Tcl_ResetResult(ip);
        return TCL_OK;

    case CONCAT:

        EXPECTARGS(2, 1, 1, "<variable with list of matrices>");

        int i, matcnt;
        Tcl_Obj **matlist;

        Tcl_Obj *varval = Tcl_ObjGetVar2(ip, objv[2], NULL, TCL_LEAVE_ERR_MSG);
        if (NULL == varval)
            return TCL_ERROR;

        if (TCL_OK != Tcl_ListObjGetElements(ip, varval, &matcnt, &matlist))
            return TCL_ERROR;

        if (0 == matcnt) {
            Tcl_ResetResult(ip);
            return TCL_OK;
        }

        int ncols = -1, nrows = 0;

        mt = NULL;
        for (i = 0; i < matcnt; i++) {
            if (TCL_OK != Tcl_ConvertToMatrix(ip, matlist[i]))
                return TCL_ERROR;
            matrixType *mt2 = matrixTypeFromTclObj(matlist[i]);
            int rows, cols;
            if (TCL_OK != Tcl_MatrixGetDimensions(ip, matlist[i], &rows, &cols))
                return TCL_ERROR;
            if (NULL == mt) {
                mt = mt2;
                ncols = cols;
            } else if (mt != mt2) {
                Tcl_SetResult(ip, "inconsistent matrix types", TCL_STATIC);
                return TCL_ERROR;
            } else if (cols != ncols) {
                Tcl_SetResult(ip, "inconsistent number of columns", TCL_STATIC);
                return TCL_ERROR;
            }
            nrows += rows;
        }

        mdat = mt->createMatrix(nrows, ncols);
        if (NULL == mdat) {
            char aux[200];
            sprintf(aux, "could not create matrix %dx%d", nrows, ncols);
            Tcl_SetResult(ip, aux, TCL_VOLATILE);
            return TCL_ERROR;
        }

        nrows = 0;
        for (i = 0; i < matcnt; i++) {
            int rows, cols;
            Tcl_MatrixGetDimensions(ip, matlist[i], &rows, &cols);
            void *m2 = matrixFromTclObj(matlist[i]);
            mt->copyRows(mdat, nrows, m2, 0, rows);
            nrows += rows;
        }

        // unset the variable to release memory
        Tcl_ObjSetVar2(ip, objv[2], NULL, Tcl_NewObj(), 0);

        Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt, mdat));
        return TCL_OK;

#if USEOPENCL
    case CLALLOC:
    case CLCREATE: {
        EXPECTARGS(2, 5, 5, "clmemobj nrows ncols matdimvar script");
        int nrows, ncols;
        stcl_context *ctx;
        if (TCL_OK != STcl_GetContext(ip,&ctx))
            return TCL_ERROR;
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[3], &nrows))
            return TCL_ERROR;
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &ncols))
            return TCL_ERROR;
        mat2 *m2 = malloc(sizeof(mat2));
        if (NULL == m2) {
            Tcl_SetResult(ip, "out of memory", TCL_STATIC);
            return TCL_ERROR;
        }
        m2->data = NULL;
        m2->rows = nrows;
        m2->cols = ncols;
        m2->ipr = IPROCO(ncols);
        int rc;
        size_t dsz = sizeof(int) * m2->ipr * m2->rows;
        cl_mem clm = clCreateBuffer(ctx->ctx,
                                    CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR |
                                        CL_MEM_HOST_READ_ONLY,
                                    dsz, NULL, &rc);
        if (rc != CL_SUCCESS) {
            SetCLErrorCode(ip, rc);
            char msg[200];
            sprintf(msg, "from clCreateBuffer with size %ld bytes", dsz);
            Tcl_AddErrorInfo(ip, msg);
            free(m2);
            return TCL_ERROR;
        }
        if (TCL_OK != STcl_CreateMemObj(ip, objv[2], clm)) {
            clReleaseMemObject(clm);
            free(m2);
            return TCL_ERROR;
        }
        Tcl_Obj *dims[3];
        dims[0] = Tcl_NewIntObj(m2->rows);
        dims[1] = Tcl_NewIntObj(m2->cols);
        dims[2] = Tcl_NewIntObj(m2->ipr * sizeof(int)/4);
        Tcl_ObjSetVar2(ip, objv[5], NULL, Tcl_NewListObj(3, dims), 0);
        clRetainMemObject(clm);
        Tcl_NRAddCallback(ip, MatrixCLCreatePostProc, m2, clm, ctx, mCmdmap[index] == CLCREATE ? 1 : 0);
        return Tcl_NREvalObj(ip, objv[6], 0);
    }
    case CLMAP: {
        EXPECTARGS(2, 4, 5,
                   "clmemobj clmemflags matrixvar matdimvar ?script?");
        stcl_context *ctx;
        if (TCL_OK != STcl_GetContext(ip,&ctx))
            return TCL_ERROR;
        int flags;
        if (TCL_OK != GetMemFlagFromTclObj(ip, objv[3], &flags))
            return TCL_ERROR;
        Tcl_Obj *matobj = TakeMatrixFromVar(ip, objv[4]);
        if (NULL == matobj)
            return TCL_ERROR;
        if (matrixTypeFromTclObj(matobj) != &stdMatrixType2) {
            Tcl_SetResult(ip, "matrixType is not stdMatrixType2", TCL_STATIC);
            return TCL_ERROR;
        }
        if (0 == ((CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR) & flags)) {
            Tcl_SetResult(
                ip,
                "neither CL_MEM_COPY_HOST_PTR nor CL_MEM_USE_HOST_PTR given",
                TCL_STATIC);
            return TCL_ERROR;
        }
        if (0 != (CL_MEM_USE_HOST_PTR & flags) && objc<7) {
            Tcl_SetResult(
                ip,
                "CL_MEM_USE_HOST_PTR only supported if script given",
                TCL_STATIC);
            return TCL_ERROR;
        }
        mat2 *m2 = (mat2 *)matrixFromTclObj(matobj);
        Tcl_Obj *dims[3];
        dims[0] = Tcl_NewIntObj(m2->rows);
        dims[1] = Tcl_NewIntObj(m2->cols);
        dims[2] = Tcl_NewIntObj(m2->ipr * sizeof(int)/4);
        Tcl_ObjSetVar2(ip, objv[5], NULL, Tcl_NewListObj(3, dims), 0);
        int rc;
        size_t dsz = sizeof(int) * m2->ipr * m2->rows;
        cl_mem clm = clCreateBuffer(ctx->ctx, flags, dsz, m2->data, &rc);
        if (rc != CL_SUCCESS) {
            SetCLErrorCode(ip, rc);
            char msg[200];
            sprintf(msg, "from clCreateBuffer with size %ld bytes", dsz);
            Tcl_AddErrorInfo(ip, msg);
            return TCL_ERROR;
        }
        if (TCL_OK != STcl_CreateMemObj(ip, objv[2], clm)) {
            clReleaseMemObject(clm);
            return TCL_ERROR;
        }
        if(objc<7) {
            return TCL_OK;
        }
        clRetainMemObject(clm);
        Tcl_NRAddCallback(ip, MatrixCLMapPostProc, matobj, clm, ctx, objv[4]);
        return Tcl_NREvalObj(ip, objv[6], 0);
    }
    case CLENQREAD:
    {
        EXPECTARGS(2, 2, 3,
                   "clmemobj matdims ?waitvars?");
        stcl_context *ctx;
        if (TCL_OK != STcl_GetContext(ip,&ctx))
            return TCL_ERROR;
        cl_mem buf;
        if (TCL_OK != STcl_GetMemObjFromObj(ip, objv[2], &buf))
            return TCL_ERROR;
        size_t dsz;
        clGetMemObjectInfo(buf,CL_MEM_SIZE,sizeof(dsz),&dsz,NULL); 
        int lobjc;
        Tcl_Obj **lobjv;
        if (TCL_OK != Tcl_ListObjGetElements(ip,objv[3],&lobjc,&lobjv))
            return TCL_ERROR;
        int nrows, ncols, ipr;
        if (3 != lobjc 
            || TCL_OK != Tcl_GetIntFromObj(ip,lobjv[0],&nrows)
            || TCL_OK != Tcl_GetIntFromObj(ip,lobjv[1],&ncols)
            || TCL_OK != Tcl_GetIntFromObj(ip,lobjv[2],&ipr)) {
            Tcl_SetResult(ip, "dimension not understood", TCL_STATIC);
            return TCL_ERROR;
        }
        cl_command_queue q = GetOrCreateCommandQueue(ip, ctx, 0);
        if (NULL == q)
            break;
        mat2 *m2 = malloc(sizeof(mat2));
        if (NULL == m2) {
            Tcl_SetResult(ip, "out of memory", TCL_STATIC);
            return TCL_ERROR;
        }
        if (NULL == (m2->data = malloc(dsz ? dsz : 1))) {
            free(m2);
            Tcl_SetResult(ip, "out of memory", TCL_STATIC);
            return TCL_ERROR;
        }
        m2->rows = nrows;
        m2->cols = ncols;
        m2->ipr = ipr;
        cl_int rc;
        int numwait = 0;
        cl_event *evtlist = NULL;
        if (objc >= 5 &&
            TCL_OK != makeWaitList(ip, objv[4], &numwait, &evtlist))
            return TCL_ERROR;
        void *data = clEnqueueMapBuffer(q, buf, 1 /* blocking */, CL_MAP_READ, 0,
                                        dsz, numwait, evtlist, NULL, &rc);
        if(CL_SUCCESS != rc || NULL == data) {
            SetCLErrorCode(ip,rc);
            Tcl_AddErrorInfo(ip, " from clEnqueueMapBuffer");
            return TCL_ERROR;
        }
        memcpy(m2->data, data, dsz);
        rc = clEnqueueUnmapMemObject(q, buf, data, 0, NULL, NULL);
        if (CL_SUCCESS != rc) {
            SetCLErrorCode(ip, rc);
            Tcl_AddErrorInfo(ip, " from clEnqueueUnmapMemObject");
            return TCL_ERROR;
        }
        Tcl_SetObjResult(ip, Tcl_NewMatrixObj(stdmatrix2, m2));
        return TCL_OK;
    }
#else
    case CLALLOC:
    case CLCREATE:
    case CLMAP:
    case CLENQREAD:
        Tcl_SetResult(ip, "library was not built with opencl support",
                      TCL_STATIC);
        return TCL_ERROR;
#endif
    }

    Tcl_SetResult(ip, "internal error in MatrixCombiCmd", TCL_STATIC);
    return TCL_ERROR;
}

int MatrixCombiCmd(ClientData cd, Tcl_Interp *ip, int objc,
                   Tcl_Obj *const objv[]) {
    return Tcl_NRCallObjProc(ip, MatrixNRECombiCmd, cd, objc, objv);
}

int Tlin_HaveType;

int Tlin_Init(Tcl_Interp *ip) {

    if (NULL == Tcl_InitStubs(ip, "8.0", 0))
        return TCL_ERROR;

    Tprime_Init(ip);

    if (!Tlin_HaveType) {
        Tptr_Init(ip);

        /* set up types and register */
        tclVector.name = "vector";
        tclVector.freeIntRepProc = VectorFreeInternalRepProc;
        tclVector.dupIntRepProc = VectorDupInternalRepProc;
        tclVector.updateStringProc = VectorUpdateStringProc;
        tclVector.setFromAnyProc = VectorSetFromAnyProc;
        Tcl_RegisterObjType(&tclVector);
        TPtr_RegObjType(TP_VECTOR, &tclVector);

        /* set up types and register */
        tclMatrix.name = "matrix";
        tclMatrix.freeIntRepProc = MatrixFreeInternalRepProc;
        tclMatrix.dupIntRepProc = MatrixDupInternalRepProc;
        tclMatrix.updateStringProc = MatrixUpdateStringProc;
        tclMatrix.setFromAnyProc = MatrixSetFromAnyProc;
        Tcl_RegisterObjType(&tclMatrix);
        TPtr_RegObjType(TP_MATRIX, &tclMatrix);

        Tlin_HaveType = 1;
    }

    int *interruptVar = malloc(sizeof(int)); /* yes, this leaks */

    Tcl_NRCreateCommand(ip, NSP "matrix", MatrixCombiCmd, MatrixNRECombiCmd,
                        (ClientData)interruptVar, NULL);

    Tcl_LinkVar(ip, NSP "interrupt", (char *)interruptVar, TCL_LINK_INT);
    Tcl_LinkVar(ip, NSP "_matCount", (char *)&matCount,
                TCL_LINK_INT | TCL_LINK_READ_ONLY);
    Tcl_LinkVar(ip, NSP "_vecCount", (char *)&vecCount,
                TCL_LINK_INT | TCL_LINK_READ_ONLY);

    Tcl_Eval(ip, "namespace eval " NSP " { namespace export * }");

    return TCL_OK;
}
