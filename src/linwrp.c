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

int  stdGetEntry(void *m, int row, int col) {
    matrix *mat = (matrix *) m;
    if ((row >= mat->rows) || (col >= mat->cols))
        return FAILIMPOSSIBLE;
    return matrix_get_entry((matrix *) mat, row, col);
}

int  stdSetEntry(void *m, int row, int col, int val) {
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

void *stdOrthoFunc(primeInfo *pi, void *inp, progressInfo *prg);

void *stdLiftFunc(primeInfo *pi, void *inp, void *lft, progressInfo *prg);

void stdQuotFunc(primeInfo *pi, void *ker, void *im, progressInfo *prg);


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


vectorType stdVectorType;
