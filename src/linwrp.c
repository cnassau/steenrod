/*
 * Wrappers for the linear algebra implementations
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

#define LINWRPC

#include <linwrp.h>

#include <linalg.h>
#include <adlin.h>

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

void stdGetDimensions(void *m, int *row, int *col) {
    matrix *mat = (matrix *) m;
    *row = mat->rows; *col = mat->cols;
}

void *stdCreateMatrix(int row, int col) {
    return matrix_create(row, col);
}

void *stdCreateCopy(void *mat) {
    return matrix_copy((matrix *) mat);
}

void stdDestroyMatrix(void *mat) {
    matrix_destroy(mat);
}

void stdClearMatrix(void *mat) {
    matrix_clear(mat);
}

void stdUnitMatrix(void *mat) {
    matrix_unit(mat);
}

void *stdOrthoFunc(primeInfo *pi, void *inp, progressInfo *prg) {
    return NULL;
}

void *stdLiftFunc(primeInfo *pi, void *inp, void *lft, progressInfo *prg) {
    return NULL;
}

void stdQuotFunc(primeInfo *pi, void *ker, void *im, progressInfo *prg) {
}

matrixType stdMatrixType = {
    .getEntry      = stdGetEntry,
    .setEntry      = stdSetEntry,
    .getDimensions = stdGetDimensions,
    .createMatrix  = stdCreateMatrix,
    .createCopy    = stdCreateCopy,
    .destroyMatrix = stdDestroyMatrix,
    .clearMatrix   = stdClearMatrix,
    .unitMatrix    = stdUnitMatrix,
    .orthoFunc     = stdOrthoFunc,
    .liftFunc      = stdLiftFunc,
    .quotFunc      = stdQuotFunc
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
    return vector_create(cols);
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
}

vectorType stdVectorType = {
    .getEntry      = &stdVGetEntry,
    .setEntry      = &stdVSetEntry,
    .getLength     = &stdVGetLength,
    .createVector  = &stdVCreateVector,
    .createCopy    = &stdVCreateCopy,
    .destroyVector = &stdVDestroyVector
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
