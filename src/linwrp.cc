/*
 * Wrappers for the linear algebra implementations
 *
 * Copyright (C) 2004-2018 Christian Nassau <nassau@nullhomotopie.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define LINWRPC

#include "linwrp.h"
#include "linalg.h"
#include "adlin.h"
#include "tlin.h"
#include <string.h>

/* The standard matrix type. This is just a wrapper around the
 * stuff from linalg.c */

int stdGetEntry(void *m, int row, int col, int *val) {
    matrix *mat = (matrix *) m;
    if ((row >= mat->rows) || (col >= mat->cols))
        return FAILIMPOSSIBLE;
    *val = matrix_get_entry((matrix *) mat, row, col);
    return SUCCESS;
}

int stdSetEntry(void *m, int row, int col, int val) {
    cint tst; int tst2;
    matrix *mat = (matrix *) m;
    if ((row >= mat->rows) || (col >= mat->cols))
        return FAILIMPOSSIBLE;
    /* first check if the value fits in a "cint" */
    tst = val; tst2 = tst;
    if (tst2 != val)
        return FAILIMPOSSIBLE;
    matrix_set_entry((matrix *) mat, row, col, val);
    return SUCCESS;
}

int stdAddToEntry(void *m, int row, int col, int val, int mod) {
    int rcode, aux;
    if (SUCCESS != (rcode = stdGetEntry(m, row, col, &aux)))
        return rcode;
    aux = aux + val;
    if (mod)
	if( (aux %= mod) < 0 ) aux += mod;
    return stdSetEntry(m, row, col, aux);
}

void stdGetDimensions(void *m, int *row, int *col) {
    matrix *mat = (matrix *) m;
    *row = mat->rows; *col = mat->cols;
}

int stdMatrixIsZero(void *m) {
    matrix *mat = (matrix *) m;
    return matrix_iszero(mat);
}

void *stdCreateMatrix(int row, int col) {
    matrix *mat = matrix_create(row, col);
    if (NULL != mat) matrix_clear(mat);
    return mat;
}

void *stdCreateMCopy(void *mat) {
    return matrix_copy((matrix *) mat);
}

int stdMatrixCopyRows(void *dst, int startrow, void *src, int from, int nrows) {
  matrix *d = (matrix*)dst, *s = (matrix*)src;
  if(d->cols != s->cols) return 0;
  if(startrow+nrows>d->rows) return 0;
  return matrix_copy_rows(d,startrow,s,from,nrows);
}

void stdDestroyMatrix(void *mat) {
    matrix_destroy((matrix*)mat);
}

void stdClearMatrix(void *mat) {
    matrix_clear((matrix*)mat);
}

void stdUnitMatrix(void *mat) {
    matrix_unit((matrix*)mat);
}

int stdReduceMatrix(void *mat, int prime) {
    matrix *m = (matrix *) mat;
    matrix_reduce(m,prime);
    return SUCCESS;
}

static int zero;

void *stdOrthoFunc(primeInfo *pi, void *inp, void *urb, int wantkernel, progressInfo *prg) {
    matrix *ker;
    Tcl_Interp *ip = NULL;
    const char *progvar = NULL;
    int pmsk = 0, *ivarp = NULL;
    if (NULL != prg) {
        ip = prg->ip; progvar = prg->progvar; pmsk = prg->pmsk; ivarp = prg->interruptVar;
    }
    ker = matrix_ortho(pi, (matrix *) inp, (matrix **) urb, ip, wantkernel, progvar, pmsk, ivarp ? ivarp : &zero);
    return ker;
}

void *stdLiftFunc(primeInfo *pi, void *inp, void *lft, void *bas, progressInfo *prg) {
    matrix *res;
    Tcl_Interp *ip = NULL;
    const char *progvar = NULL;
    int pmsk = 0, *ivarp = NULL;
    if (NULL != prg) { ip = prg->ip; progvar = prg->progvar; pmsk = prg->pmsk;  ivarp = prg->interruptVar;}
    res = matrix_lift(pi, (matrix *) inp, (matrix*)lft, (matrix*)bas, ip, progvar, pmsk, ivarp ? ivarp : &zero);
    return res;
}

void stdQuotFunc(primeInfo *pi, void *ker, void *im, progressInfo *prg) {
    Tcl_Interp *ip = NULL;
    const char *progvar = NULL;
    int pmsk = 0, *ivarp = NULL;
    if (NULL != prg) { ip = prg->ip; progvar = prg->progvar; pmsk = prg->pmsk;  ivarp = prg->interruptVar;}
    matrix_quotient(pi, (matrix *) ker, (matrix *) im, ip, progvar, pmsk, ivarp ? ivarp : &zero);
}

int stdMAdd(void *vv1, void *vv2, int scale, int mod) {
    matrix *v1 = (matrix *) vv1;
    matrix *v2 = (matrix *) vv2;

    if ((v1->rows != v2->rows) || (v1->cols != v2->cols))
        return FAILIMPOSSIBLE;

    matrix_add(v1, v2, scale, mod);

    return SUCCESS;
}

matrixType stdMatrixType = {
    .name          = "stdmatrix",
    .getEntry      = stdGetEntry,
    .setEntry      = stdSetEntry,
    .addToEntry    = stdAddToEntry,
    .getDimensions = stdGetDimensions,
    .createMatrix  = stdCreateMatrix,
    .createCopy    = stdCreateMCopy,
    .destroyMatrix = stdDestroyMatrix,
    .clearMatrix   = stdClearMatrix,
    .unitMatrix    = stdUnitMatrix,
    .reduce        = stdReduceMatrix,
    .iszero        = stdMatrixIsZero,
    .shrinkRows    = NULL,
    .add           = stdMAdd,
    .orthoFunc     = stdOrthoFunc,
    .liftFunc      = stdLiftFunc,
    .quotFunc      = stdQuotFunc,
    .copyRows      = stdMatrixCopyRows
};

int stdVGetEntry(void *vec, int idx, int *val) {
    vector *v = (vector *) vec;
    if (idx >= (v->num)) return FAILIMPOSSIBLE;
    *val = vector_get_entry(v, idx);
    return SUCCESS;
}

int stdVSetEntry(void *vec, int idx, int val) {
    vector *v = (vector *) vec;
    cint tst; int tst2;
    /* first check if the value fits in a "cint" */
    tst = val; tst2 = tst;
    if (tst2 != val)
        return FAILIMPOSSIBLE;
    if (idx >= (v->num)) return FAILIMPOSSIBLE;
    vector_set_entry(v, idx, val);
    return SUCCESS;
}

int stdVGetLength(void *vec) {
    vector *src = (vector *) vec;
    return src->num;
}

void *stdVCreateVector(int cols) {
    vector *res = vector_create(cols);
    if (NULL != res) vector_clear(res);
    return res;
}

void *stdVCreateCopy(void *vec) {
    vector *src = (vector *) vec;
    vector *res = vector_create(src->num);
    if (NULL == res) return NULL;
    vector_copy(res, src);
    return res;
}

void stdVDestroyVector(void *vec) {
    vector_dispose((vector *) vec);
    freex(vec);
}

int stdVReduce(void *vec, int prime) {
    vector *v = (vector *) vec;
    vector_reduce(v,prime);
    return SUCCESS;
}

int stdVAdd(void *vv1, void *vv2, int scale, int mod) {
    vector *v1 = (vector *) vv1;
    vector *v2 = (vector *) vv2;

    if (v1->num != v2->num)
        return FAILIMPOSSIBLE;

    vector_add(v1, v2, scale, mod);

    return SUCCESS;
}

vectorType stdVectorType = {
    .name          = "stdvector",
    .getEntry      = &stdVGetEntry,
    .setEntry      = &stdVSetEntry,
    .getLength     = &stdVGetLength,
    .createVector  = &stdVCreateVector,
    .createCopy    = &stdVCreateCopy,
    .destroyVector = &stdVDestroyVector,
    .add           = &stdVAdd,
    .reduce        = &stdVReduce
};

typedef struct rcvect {
    Tcl_Obj *matobj;
    matrixType *mtype;
    void *mdata;
    int rowcolnum;
    int isrow;
} rcvect;

void *CreateMatrixRowcolVector(Tcl_Obj *matobj, int rcnum, int isrow) {
    rcvect *ans = (rcvect*) malloc(sizeof(rcvect));
    ans->matobj = matobj;
    Tcl_IncrRefCount(matobj);
    ans->mtype = matrixTypeFromTclObj(matobj);
    ans->mdata = matrixFromTclObj(matobj);
    ans->rowcolnum = rcnum;
    ans->isrow = isrow;
    return ans;
}

int rcGetEntry(void *vec, int idx, int *val) {
    rcvect *r = (rcvect*)vec;
    int nr, nc;
    if(r->isrow) {
      nr = r->rowcolnum;
      nc = idx;
    } else {
      nc = r->rowcolnum;
      nr = idx;
    }
    return (r->mtype)->getEntry(r->mdata,nr,nc,val);
}

int rcGetLength(void*vec) {
    rcvect *r = (rcvect*)vec;
    int nr,nc;
    (r->mtype)->getDimensions(r->mdata,&nr,&nc);
    return r->isrow ? nc : nr;
}

void *rcCreateCopy(void*vec) {
    rcvect *r = (rcvect*)vec;
    rcvect *c = (rcvect*)malloc(sizeof(rcvect));
    memcpy(c,r,sizeof(rcvect));
    Tcl_IncrRefCount(c->matobj);
    return c;
}

void rcDestroyVector(void*vec) {
    rcvect *r = (rcvect*)vec;
    Tcl_DecrRefCount(r->matobj);
    free(r);
}

vectorType matrixRowcolVector = {
    .name          = "row/col vector",
    .getEntry      = &rcGetEntry,
    .setEntry      = NULL, /* SetEntry*/
    .getLength     = &rcGetLength,
    .createVector  = NULL, /* CreateVector */
    .createCopy    = &rcCreateCopy,
    .destroyVector = &rcDestroyVector,
    .add           = NULL,
    .reduce        = NULL
};

void *createStdMatrixCopy(matrixType *mt, void *mat) {
    int rows, cols, i, j, val;
    void *res;
    (mt->getDimensions)(mat, &rows, &cols);
    res = (stdmatrix->createMatrix)(rows, cols);
    if (NULL == res) return NULL;
    for (i=0;i<rows;i++)
        for (j=0;j<cols;j++) {
            if (SUCCESS != (mt->getEntry)(mat,i,j,&val)) {
                (stdmatrix->destroyMatrix)(res);
                return NULL;
            }
            if (SUCCESS != (stdmatrix->setEntry)(res,i,j,val)) {
                (stdmatrix->destroyMatrix)(res);
                return NULL;
            }
        }
    return res;
}

void *createStdVectorCopy(vectorType *vt, void *vec) {
    int len, i, val;
    void *res;
    len = (vt->getLength)(vec);
    res = (stdvector->createVector)(len);
    if (NULL == res) return NULL;
    for (i=0;i<len;i++) {
        if (SUCCESS != (vt->getEntry)(vec,i,&val)) {
            (stdvector->destroyVector)(res);
            return NULL;
        }
        if (SUCCESS != (stdvector->setEntry)(res,i,val)) {
            (stdvector->destroyVector)(res);
            return NULL;
        }
    }
    return res;
}

int LAVadd(vectorType **vt1, void **vdat1,
           vectorType *vt2, void *vdat2, int scale, int mod) {

    if ((vt2 == *vt1) && (NULL != vt2->add))
        return vt2->add(*vdat1, vdat2, scale, mod);

    ASSERT(NULL == "LAVadd needs to be enhanced");

    return FAIL;
}

int MatrixAddNaive(matrixType *mt1, void *mdat1,
                    matrixType *mt2, void *mdat2, int scale, int mod) {
    int rows, cols, i, j;
    mt1->getDimensions(mdat1,&rows,&cols);
    mt2->getDimensions(mdat2,&i,&j);
    if ((rows != i) || (cols != j))
        return FAILIMPOSSIBLE;
    for (i=0;i<rows;i++)
        for (j=0;j<cols;j++) {
            int val;
            mt2->getEntry(mdat2,i,j,&val);
            mt1->addToEntry(mdat1,i,j,val*scale,mod);
        }
    return SUCCESS;
}

int LAMadd(matrixType **vt1, void **vdat1,
           matrixType *vt2, void *vdat2, int scale, int mod) {

    /* make sure that we only use mod 2 arithmetic if mod == 2 */
    if (2 == mod) {
        if (0 == (0x1 & scale)) return SUCCESS;
        if ((stdmatrix2 == vt2) && (stdmatrix2 == *vt1)) {
            return stdmatrix2->add(*vdat1, vdat2, scale, mod);
        }
        return MatrixAddNaive(*vt1, *vdat1, vt2, vdat2, scale, mod);
    }

    if (stdmatrix != *vt1) {
        void *newdat = createStdMatrixCopy(*vt1,*vdat1);
        if (NULL == newdat) return FAILMEM;
        (*vt1)->destroyMatrix(*vdat1);
        *vt1 = stdmatrix;
        *vdat1 = newdat;
    }

    if ((vt2 == *vt1) && (NULL != vt2->add))
        return vt2->add(*vdat1, vdat2, scale, mod);

    return MatrixAddNaive(*vt1, *vdat1, vt2, vdat2, scale, mod);
}

int vectorIsZero(vectorType *vt, void *vdat) {
    int i, num;
    if (NULL != vt->iszero)
        return vt->iszero(vdat);
    num = vt->getLength(vdat);
    for (i=num;i--;) {
        int val;
        vt->getEntry(vdat, i, &val);
        if (val) return 0;
    }
    return 1;
}

int matrixIsZero(matrixType *mt, void *mdat) {
    int i,j,r,c;
    if (NULL != mt->iszero)
        return mt->iszero(mdat);
    mt->getDimensions(mdat, &r, &c);
    for (i=r;i--;)
        for (j=c;j--;) {
            int val;
            mt->getEntry(mdat, i, j, &val);
            if (val) return 0;
        }
    return 1;
}
