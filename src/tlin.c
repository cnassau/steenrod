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

#include "steenrod.h"
#include "tlin.h"
#include "adlin.h"
#include <string.h>

#define FREETCLOBJ(obj) { INCREFCNT(obj); DECREFCNT(obj); }

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

static int vecCount;

#if 0
#  define INCVECCNT \
  { fprintf(stderr, "vecCount = %d (%s, %d)\n", ++vecCount, __FILE__, __LINE__); }
#  define DECVECCNT \
  { fprintf(stderr, "vecCount = %d (%s, %d)\n", --vecCount, __FILE__, __LINE__); }
#else
#  define INCVECCNT { ++vecCount; }
#  define DECVECCNT { --vecCount; }
#endif


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
    INCVECCNT;

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
    INCVECCNT;
}

/**************************************************************************
 *
 * The Tcl object type for matrices represents them as a list of list of integers.
 *
 */

static int matCount;

#if 0
#  define INCMATCNT \
  { fprintf(stderr, "matCount = %d (%s, %d)\n", ++matCount, __FILE__, __LINE__); }
#  define DECMATCNT \
  { fprintf(stderr, "matCount = %d (%s, %d)\n", --matCount, __FILE__, __LINE__); }
#else
#  define INCMATCNT { ++matCount; }
#  define DECMATCNT { --matCount; }
#endif

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
    INCMATCNT;
    return res;
}

void MatrixFreeInternalRepProc(Tcl_Obj *obj) {
    matrixType *vt = matrixTypeFromTclObj(obj);
    (vt->destroyMatrix)(matrixFromTclObj(obj));
    DECMATCNT;
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
        ASSERT(0 == rows);
        mat = (stdmatrix->createMatrix)(0,0);
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
    INCMATCNT;
}

/**** utilities */

int Tcl_MatrixGetDimensions(Tcl_Interp *ip, Tcl_Obj *obj, int *rows, int *cols) {
    matrixType *mt;
    if (TCL_OK != Tcl_ConvertToMatrix(ip, obj)) return TCL_ERROR;
    mt = PTR1(obj);
    (mt->getDimensions)(PTR2(obj), rows, cols);
    return TCL_OK;
}

/**** wrappers for the adlin routines  */

#define FAILASSERT(msg) { Tcl_SetResult(ip,msg,TCL_VOLATILE); return NULL; }

Tcl_Obj *Tcl_LiftCmd(primeInfo *pi, Tcl_Obj *inp, Tcl_Obj *lft, Tcl_Obj *bas,
                     Tcl_Interp *ip, const char *progvar, int pmsk, int *interruptVar) {
    matrixType *mt = PTR1(inp), *mt2 = PTR1(lft);
    int krows, kcols, irows, icols;
    void *rdat;
    progressInfo pro;

    if(NULL == bas) {
      if(Tcl_IsShared(inp))
        FAILASSERT("inp must not be shared in Tcl_OrthoCmd!");
    }

    /* since we cannibalize lft, it must not be shared */
    if (Tcl_IsShared(lft))
        FAILASSERT("lft must not be shared in Tcl_OrthoCmd!");

    if ((NULL == (mt->liftFunc)))
        FAILASSERT("matrix type has no lift func");

    if ((mt != mt2)) {
        FAILASSERT("matrix types differ");
    }

    if (PTR2(inp) == PTR2(lft))
        FAILASSERT(NULL == "lifting matrix through itself not yet supported");

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
    pro.interruptVar = interruptVar;

    Tcl_InvalidateStringRep(lft);
    if(NULL == bas) Tcl_InvalidateStringRep(inp);
    rdat = (mt->liftFunc)(pi, PTR2(inp), PTR2(lft), bas ? PTR2(bas) : NULL, (NULL != progvar) ? &pro : NULL);

    if (NULL == rdat) return NULL;

    return Tcl_NewMatrixObj(mt, rdat);
}

int Tcl_QuotCmd(primeInfo *pi, Tcl_Obj *ker, Tcl_Obj *inp,
                     Tcl_Interp *ip, const char *progvar, int pmsk, int *interruptVar) {
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
            PTR1(ker) = stdmatrix; PTR2(ker) = newker;
            mt2 = stdmatrix;
        }
        if (mt != stdmatrix) {
            void *newim = createStdMatrixCopy(mt, PTR2(inp));
            mt->destroyMatrix(PTR2(inp));
            PTR1(inp) = stdmatrix; PTR2(inp) = newim;
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

    if (ker==inp) ASSERT(NULL == "quot not fully implemented");

    Tcl_InvalidateStringRep(ker);
    (mt->quotFunc)(pi, PTR2(ker), PTR2(inp), (NULL != progvar) ? &pro : NULL);

    return TCL_OK;
}

Tcl_Obj *Tcl_OrthoCmd(primeInfo *pi, Tcl_Obj *inp, Tcl_Obj **urb,
                      Tcl_Interp *ip, int wantkernel,
		      const char *progvar, int pmsk, int *interruptVar) {
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
    if (NULL != mt->reduce) (mt->reduce)(PTR2(inp), pi->prime);

    if (NULL != urb) {
        oth2 = &oth;
    } else {
        oth2 = NULL;
    }

    res = (mt->orthoFunc)(pi, PTR2(inp), oth2, wantkernel, (NULL != progvar) ? &pro : NULL);

    if (NULL != urb) {
        *urb = Tcl_NewMatrixObj(mt, oth);
    }

    Tcl_InvalidateStringRep(inp);

    if (NULL == res) return NULL;

    return Tcl_NewMatrixObj(mt, res);
}

#undef RETERR
#define RETERR(msg) \
{ if (NULL!=ip) Tcl_SetResult(ip,msg,TCL_VOLATILE); return TCL_ERROR; }

/* VAddCmd tries to do "(*obj1) += (*obj2) mod modval" */

int VAddCmd(Tcl_Interp *ip, Tcl_Obj *obj1, Tcl_Obj *obj2, int scale, int modval) {
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

    if (d1 != d2) RETERR("dimensions don't match");

    Tcl_InvalidateStringRep(obj1);

    aux = (vectorType *) PTR1(obj1);
    if (SUCCESS != LAVadd(&aux, &PTR2(obj1),
                          vt2, vdat2, scale, modval))
        RETERR("could not add vectors (LAVadd failed)");
    PTR1(obj1) = aux;

    return TCL_OK;
}

/* MAddCmd tries to do "(*obj1) += (*obj2) mod modval" */

int MAddCmd(Tcl_Interp *ip, Tcl_Obj *obj1, Tcl_Obj *obj2, int scale, int modval) {
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

    if ((r1 != r2) || (c1 != c2)) RETERR("dimensions don't match");

    Tcl_InvalidateStringRep(obj1);

    aux = (matrixType *) PTR1(obj1);
    if (SUCCESS != LAMadd(&aux, &PTR2(obj1),
                          mt2, mdat2, scale, modval))
        RETERR("could not add matrices (LAMadd failed)");
    PTR1(obj1) = aux;

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
	if(idx<0) {
	  Tcl_SetResult(ip,"negative index",TCL_STATIC);
	  mt2->destroyMatrix(mdat2);
	  return TCL_ERROR;
	} else if (idx>=ic) {
	  Tcl_SetResult(ip,"index too big",TCL_STATIC);
	  mt2->destroyMatrix(mdat2);
	  return TCL_ERROR;
	}
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
        mdat2 = mt1->shrinkRows(mdat1, ind, num);

        if (NULL == mdat2) RETERR("out of memory");

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
    if (NULL == Tcl_ObjSetVar2(ip, varname, NULL, Tcl_NewObj(), TCL_LEAVE_ERR_MSG)) {
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
    if (NULL == Tcl_ObjSetVar2(ip, varname, NULL, Tcl_NewObj(), TCL_LEAVE_ERR_MSG)) {
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

/**** Encoding and decoding ********************************************************/

typedef enum { ENCHEX } enctype;

static CONST char *encnames[] = { "hex", (char *) NULL };

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
    if (result != TCL_OK) return result;

    if (TCL_OK != Tcl_GetIntFromObj(ip, objv[1], &blocksize))
        return TCL_ERROR;

    switch (blocksize) {
        case 1: mask = 0x01; break;
        case 2: mask = 0x03; break;
        case 4: mask = 0x0f; break;
        case 8: mask = 0xff; break;
        default:
            Tcl_SetResult(ip, "Unsupported blocksize: should be 1, 2, 4, or 8", TCL_STATIC);
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

    if (NULL == mdat) RETERR("out of memory");

    if (0 == ncols) {
        Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt, mdat));
        return TCL_OK;
    }

    for (i=0;i<nrows;i++) {
        Tcl_Obj *rowPtr;
        char *row;
        if (TCL_OK != Tcl_ListObjIndex(ip, objv[4], i, &rowPtr))
            ASSERT(NULL == "unexpected error in TclDecodeCmd (I)");
        if (NULL == rowPtr)
            ASSERT(NULL == "unexpected error in TclDecodeCmd (II)");
        row = Tcl_GetString(rowPtr);

        for (j=0;j<ncols;) {
            unsigned char h, l, x, v;
            if (0 == (h = *row++)) break;
            if (0 == (l = *row++)) break;

#define FROMHEX(ch) (((ch) >= 'a') ? ((ch) - 'a' + 10) : ((ch) - '0'))

            x = FROMHEX(h); x <<= 4; x |= FROMHEX(l);

#define EXTRACT(sz) {v = (x>>(8-sz))&mask; if (j>=ncols) break; mt->setEntry(mdat,i,j++,v); x<<=sz;}

            switch (blocksize) {
                case 1:
                    EXTRACT(1); EXTRACT(1); EXTRACT(1); EXTRACT(1);
                    EXTRACT(1); EXTRACT(1); EXTRACT(1); EXTRACT(1);
                    break;
                case 2:
                    EXTRACT(2); EXTRACT(2); EXTRACT(2); EXTRACT(2);
                    break;
                case 4:
                    EXTRACT(4); EXTRACT(4);
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

    if ((base>1) && (base<=2)) {
        blocksize = 1;
    } else if ((base>2) && (base<=4)) {
        blocksize = 2;
    } else if ((base>4) && (base<=16)) {
        blocksize = 4;
    } else if ((base>16) && (base<=256)) {
        blocksize = 8;
    }

    if (blocksize < 0) {
        Tcl_SetResult(ip, "base must be between 2 and 255", TCL_STATIC);
        return TCL_ERROR;
    }

    mask = (1 << blocksize) - 1;

    mt   = matrixTypeFromTclObj(mat);
    mdat = matrixFromTclObj(mat);

    mt->getDimensions(mdat, &nrows, &ncols);

    /* Calculate and allocate necessary space */

    len  = nrows;                       /* number of spaces as separators + one newline */
    rcs = (7 + ncols * blocksize) / 8;  /* the number of bytes per row */
    rcs *= 2;                           /* now the number of hex digits per row */
    len += nrows * rcs;

    if (NULL == (enc = Tcl_AttemptAlloc(len+1))) {
        Tcl_SetResult(ip, "out of memory", TCL_STATIC);
        return TCL_ERROR;
    }

    wrk = enc;

#define TOHEX(z) (((z)<10) ? ('0'+(z)) : ('a'+(z)-10))

    for (i=0;i<nrows;i++) {
        if (i) *wrk++ = ' ';

        for (j=0;j<ncols;) {
            unsigned char c, l, h;
            c = 0;
            switch (blocksize) {

#define NEXT(sz) {c<<=sz; if(j>=ncols) val=0; else mt->getEntry(mdat,i,j++,&val); c |= (val&mask);}

                case 1:
                    NEXT(1); NEXT(1); NEXT(1); NEXT(1); NEXT(1); NEXT(1); NEXT(1); NEXT(1);
                    break;
                case 2:
                    NEXT(2); NEXT(2); NEXT(2); NEXT(2);
                    break;
                case 4:
                    NEXT(4); NEXT(4);
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
    res[4]->length = len-1;
    res[4]->bytes = enc;

    Tcl_SetObjResult(ip, Tcl_NewListObj(5, res));
    return TCL_OK;
}

int Tcl_Convert2Cmd(Tcl_Interp *ip, Tcl_Obj *inmat) {
    void *res, *mdat;
    matrixType *mt;
    int i,j,rows,cols;

    if (TCL_OK != Tcl_ConvertToMatrix(ip, inmat))
        return TCL_ERROR;

    mt   = matrixTypeFromTclObj(inmat);
    mdat = matrixFromTclObj(inmat);

    mt->getDimensions(mdat,&rows,&cols);

    res = stdmatrix2->createMatrix(rows,cols);

    if (NULL == res) {
        Tcl_SetResult(ip, "Out of memory", TCL_STATIC);
        return TCL_ERROR;
    }

    for (i=rows;i--;) {
        for (j=cols;j--;) {
            int val;
            mt->getEntry(mdat,i,j,&val);
            if ((val != 0) && (val != 1)) {
                char errmsg[80];
                sprintf(errmsg, "all entries must be 0 or 1 (found %d)", val);
                stdmatrix2->destroyMatrix(res);
                Tcl_SetResult(ip, errmsg, TCL_VOLATILE);
                return TCL_ERROR;
            }
            stdmatrix2->setEntry(res,i,j,val);
        }
    }

    Tcl_SetObjResult(ip, Tcl_NewMatrixObj(stdmatrix2, res));
    return TCL_OK;
}

int Tcl_MultMatrixCmd(Tcl_Interp *ip, primeInfo *pi,
                      Tcl_Obj *f1, Tcl_Obj *f2) {

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

    for (i=0;i<rows;i++) {
        for (j=0;j<cols;j++) {
            int aux = 0;
            for (k=0;k<inb;k++) {
                int v,w;
                mt1->getEntry(mdat1,i,k,&v);
                mt2->getEntry(mdat2,k,j,&w);
                aux += v*w; aux %= p;
            }
            mt1->setEntry(mres,i,j,aux);
        }
    }

    Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt1, mres));
    return TCL_OK;
}

/**** Implementation of the matrix combi-command ***********************************/

#define LINALG_INTERRUPT_VARIABLE (*interruptVar)

#define CHECK_INTERRUPT_VARIABLE(where)				\
    if (LINALG_INTERRUPT_VARIABLE) {					\
	Tcl_SetResult(ip, "computation has been interrupted", TCL_STATIC); \
	return TCL_ERROR; }

static CONST char *rcnames[] = { "rows", "cols", "single-row", "single-column", NULL };

typedef enum { ORTHO, LIFT, LIFTV, QUOT, DIMS, CREATE, ADDTO,
               ISZERO, TEST, EXTRACT, ENCODE64, DECODE,
               TYPE, CONVERT2, MULT, UNIT } matcmdcode;

static CONST char *mCmdNames[] = { "orthonormalize", "lift", "liftvar",
                                   "quotient", "extract", "dimensions",
                                   "create", "addto", "iszero",
                                   "test", "encode64", "decode",
                                   "type", "convert2", "multiply",
                                   "unit",
                                   (char *) NULL };

static matcmdcode mCmdmap[] = { ORTHO, LIFT, LIFTV, QUOT, EXTRACT,
                                DIMS, CREATE, ADDTO, ISZERO, TEST, ENCODE64, DECODE,
                                TYPE, CONVERT2, MULT, UNIT };

int MatrixCombiCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[]) {
    int result, index, scale, modval, rows, cols;
    primeInfo *pi;
    matrixType *mt; void *mdat;
    Tcl_Obj *(varp[5]), *urbobj, **urb;
    int *interruptVar = (int *) cd;
    LINALG_INTERRUPT_VARIABLE = 0;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(ip, objv[1], mCmdNames, "subcommand", 0, &index);
    if (result != TCL_OK) return result;

#define EXPECTARGS(bas,min,max,msg) {                 \
   if ((objc<((bas)+(min))) || (objc>((bas)+(max)))) { \
       Tcl_WrongNumArgs(ip, (bas), objv, msg);        \
       return TCL_ERROR; } }

    switch (mCmdmap[index]) {
        case TYPE:
            EXPECTARGS(2, 1, 1, "<matrix>");

            if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[2]))
                return TCL_ERROR;

            Tcl_SetResult(ip, (char *) matrixTypeFromTclObj(objv[2])->name, TCL_VOLATILE);
            return TCL_OK;

        case CONVERT2:
            EXPECTARGS(2, 1, 1, "<matrix>");

            return Tcl_Convert2Cmd(ip, objv[2]);

        case ENCODE64:
            EXPECTARGS(2, 2, 2, "<base> <matrix>");

            if (TCL_OK != Tcl_GetIntFromObj(ip,objv[2],&scale))
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

            if (TCL_OK != Tcl_GetPrimeInfo(ip,objv[2],&pi))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[4]))
                return TCL_ERROR;

            if (NULL == (varp[1] = TakeMatrixFromVar(ip, objv[3])))
                return TCL_ERROR;

            if (TCL_OK != Tcl_QuotCmd(pi, varp[1], objv[4], ip, THEPROGVAR, theprogmsk, interruptVar)) {
                DECREFCNT(varp[1]);
		CHECK_INTERRUPT_VARIABLE(quot);
		return TCL_ERROR;
            }

            /* put result into var1 */
            if (NULL == Tcl_ObjSetVar2(ip, objv[3], NULL, varp[1], TCL_LEAVE_ERR_MSG)) {
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

            varp[3] = Tcl_LiftCmd(pi, objv[3], varp[2], objv[4], ip, THEPROGVAR, theprogmsk, interruptVar);

            if (NULL == varp[3]) {
                DECREFCNT(varp[2]);
		CHECK_INTERRUPT_VARIABLE(lift);
                return TCL_ERROR;
            }

            if (NULL == Tcl_ObjSetVar2(ip, objv[5], NULL, varp[2], TCL_LEAVE_ERR_MSG)) {
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

            varp[3] = Tcl_LiftCmd(pi, varp[4], varp[2], NULL, ip, THEPROGVAR, theprogmsk, interruptVar);

            if (NULL == varp[3]) {
                DECREFCNT(varp[4]);
                DECREFCNT(varp[2]);
		CHECK_INTERRUPT_VARIABLE(lift);
                return TCL_ERROR;
            }

            if (NULL == Tcl_ObjSetVar2(ip, objv[4], NULL, varp[2], TCL_LEAVE_ERR_MSG)) {
                DECREFCNT(varp[4]);
                DECREFCNT(varp[2]);
                return TCL_ERROR;
            }

	    DECREFCNT(varp[4]);
            DECREFCNT(varp[2]);

            Tcl_SetObjResult(ip, varp[3]);
            return TCL_OK;

        case ORTHO:
	  {
	    int wantkernel;
            EXPECTARGS(2, 3, 4, "<prime> <inputvar> <kernelvar> ?<basisvar>?");

            if (TCL_OK != Tcl_GetPrimeInfo(ip,objv[2],&pi))
                return TCL_ERROR;

            if (NULL == (varp[1] = TakeMatrixFromVar(ip, objv[3])))
                return TCL_ERROR;

            if (objc > 5) {
                urb = &urbobj;
            } else {
                urb = NULL;
            }

	    wantkernel = (0 != *Tcl_GetString(objv[4]));

            varp[2] = Tcl_OrthoCmd(pi, varp[1], urb, ip, wantkernel, THEPROGVAR, theprogmsk, interruptVar);

            if (wantkernel && NULL == varp[2]) {
                DECREFCNT(varp[1]);
		CHECK_INTERRUPT_VARIABLE(ortho);
                RETERR("orthonormalization failed");
            }

            /* set variables and return */
            if (NULL == Tcl_ObjSetVar2(ip, objv[3], NULL,
                                       varp[1], TCL_LEAVE_ERR_MSG)) {
                DECREFCNT(varp[1]);
                DECREFCNT(varp[2]);
                return TCL_ERROR;
            }
            DECREFCNT(varp[1]);

            if (wantkernel && (NULL == Tcl_ObjSetVar2(ip, objv[4], NULL, varp[2], TCL_LEAVE_ERR_MSG))) {
                DECREFCNT(varp[2]);
                return TCL_ERROR;
            }

            if ((NULL != urb) &&
                (NULL == Tcl_ObjSetVar2(ip, objv[5], NULL,
                                        urbobj, TCL_LEAVE_ERR_MSG))) {
                return TCL_ERROR;
            }

            return TCL_OK;
	  }
        case EXTRACT:
            EXPECTARGS(2, 3, 3, "rows/cols <matrix> <list of indices>");

            if (TCL_OK != Tcl_GetIndexFromObj(ip, objv[2], rcnames,
                                              "option", 0, &index))
                return TCL_ERROR;

            if (TCL_OK != Tcl_ConvertToMatrix(ip, objv[3]))
                return TCL_ERROR;

	    if(index<2) {

            if (TCL_OK != Tcl_ConvertToIntList(ip, objv[4]))
                return TCL_ERROR;

            return index ?
	      ExtractColsCmd(ip, objv[3], ILgetIntPtr(objv[4]),
			     ILgetLength(objv[4]))
	      : ExtractRowsCmd(ip, objv[3], ILgetIntPtr(objv[4]),
			       ILgetLength(objv[4]));
	    } else {
	      /* single row/column variant */

              int nrc;
              if(TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &nrc))
                 return TCL_ERROR;

	      void *vdat = CreateMatrixRowcolVector(objv[3],nrc,(index==2)?1:0);
              Tcl_SetObjResult(ip, Tcl_NewVectorObj(&matrixRowcolVector,vdat));
              return TCL_OK;
	    }
        case MULT:
            EXPECTARGS(2, 3, 3, "<prime> <matrix1> <matrix2>");

            if (TCL_OK != Tcl_GetPrimeInfo(ip,objv[2],&pi))
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

            if (NULL == mdat) RETERR("out of memory");

            Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt, mdat));
            return TCL_OK;

        case UNIT:
            EXPECTARGS(2, 1, 1, "<rows>");

            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[2], &rows))
                return TCL_ERROR;

            mt = stdmatrix;
            mdat = mt->createMatrix(rows, rows);

            if (NULL == mdat) RETERR("out of memory");

            for (cols=0; cols < rows; cols++) {
                mt->setEntry(mdat,cols,cols,1);
            }

            Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mt, mdat));
            return TCL_OK;

        case ADDTO:
            EXPECTARGS(2, 2, 4, "<varname> <matrix> ?<scale>? ?<mod>?");

            scale = 1; modval = 0;

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

            if (NULL == Tcl_ObjSetVar2(ip, objv[2], NULL,
                                       varp[1], TCL_LEAVE_ERR_MSG)) {
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

    }

    Tcl_SetResult(ip, "internal error in MatrixCombiCmd", TCL_STATIC);
    return TCL_ERROR;
}

int Tlin_HaveType;

int Tlin_Init(Tcl_Interp *ip) {

    if (NULL == Tcl_InitStubs(ip, "8.0", 0)) return TCL_ERROR;

    Tprime_Init(ip);

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

    int *interruptVar = malloc(sizeof(int)); /* yes, this leaks */

    Tcl_CreateObjCommand(ip, NSP "matrix", MatrixCombiCmd, (ClientData) interruptVar, NULL);

    Tcl_LinkVar(ip, NSP "interrupt", (char *) interruptVar, TCL_LINK_INT);
    Tcl_LinkVar(ip, NSP "_matCount", (char *) &matCount, TCL_LINK_INT | TCL_LINK_READ_ONLY);
    Tcl_LinkVar(ip, NSP "_vecCount", (char *) &vecCount, TCL_LINK_INT | TCL_LINK_READ_ONLY);

    Tcl_Eval(ip, "namespace eval " NSP " { namespace export * }");

    return TCL_OK;
}
