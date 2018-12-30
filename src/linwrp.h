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

#ifndef LINWRP_DEF
#define LINWRP_DEF

#include "prime.h"
#include <tcl.h>

typedef struct {
    Tcl_Interp *ip;
    const char *progvar;
    int         pmsk;
    int        *interruptVar;
} progressInfo;

typedef struct {
    const char *name;

    int  (*getEntry)(void *vec, int idx, int *val);
    int  (*setEntry)(void *vec, int idx, int val);
    int  (*getLength)(void *vec);
    void *(*createVector)(int cols);
    void *(*createCopy)(void *vec);
    void (*destroyVector)(void *vec);
    int  (*add)(void *v1, void *v2, int scale, int mod);
    int  (*reduce)(void *mat, int prime);
    int  (*iszero)(void *mat);
} vectorType;

typedef struct {
    const char *name;

    int  (*getEntry)(void *mat, int row, int col, int *val);
    int  (*setEntry)(void *mat, int row, int col, int val);
    int  (*addToEntry)(void *mat, int row, int col, int val, int mod);
    void (*getDimensions)(void *mat, int *row, int *col);
    void *(*createMatrix)(int row, int col);
    void *(*createCopy)(void *mat);
    void (*destroyMatrix)(void *mat);
    void (*clearMatrix)(void *mat);
    void (*unitMatrix)(void *mat);
    int  (*reduce)(void *mat, int prime);
    int  (*iszero)(void *mat);
    void *(*shrinkRows)(void *mat, int *idx, int num);
    int  (*add)(void *v1, void *v2, int scale, int mod);
  void *(*orthoFunc)(primeInfo *pi, void *inp, void *urb, int wantkernel, progressInfo *prg);
  /* liftFunc computes lifts for the vectors in lft through the matrix inp.
   * it has two modes of operation:
   *   1) NULL == bas : orthonormalizes *inp, computes lift
   *   2) NULL != bas : assumes *inp has been orthonormalized using the basis *bas
   */
  void *(*liftFunc)(primeInfo *pi, void *inp, void *lft, void *bas, progressInfo *prg);
    void (*quotFunc)(primeInfo *pi, void *ker, void *im, progressInfo *prg);
  int (*copyRows)(void *dst, int startrow, void *src, int from, int nrows);
} matrixType;

#ifndef LINWRPC
extern matrixType stdMatrixType;
extern vectorType stdVectorType;
#endif
#ifndef LINWRPC2
// optimized versions for the prime 2
extern matrixType stdMatrixType2;
extern vectorType stdVectorType2;
#endif

#define stdmatrix (&(stdMatrixType))
#define stdvector (&(stdVectorType))
#define stdmatrix2 (&(stdMatrixType2))
#define stdvector2 (&(stdVectorType2))

extern vectorType matrixRowcolVector;

void *CreateMatrixRowcolVector(Tcl_Obj *matrix, int rcnum, int roworcol);

void *createStdMatrixCopy(matrixType *mt, void *mat);
void *createStdVectorCopy(vectorType *vt, void *vec);

int LAVadd(vectorType **vt1, void **vec1,
           vectorType *vt2, void *vec2, int scale, int mod);

int LAMadd(matrixType **vt1, void **vec1,
           matrixType *vt2, void *vec2, int scale, int mod);

int vectorIsZero(vectorType *vt, void *vdat);
int matrixIsZero(matrixType *mt, void *mdat);

/* selected internals for the 2-primary implementation */

#define BITSPERINT (sizeof(int) * 8)
#define IPROCO(cols) ((BITSPERINT-1+(cols))/BITSPERINT)

typedef struct {
    int *data;
    int size, ints;
} vec2;

typedef struct {
    int *data;
    int rows, cols;
    int ipr; /* ints per row */
} mat2;

#endif
