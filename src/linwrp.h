/*
 * Wrappers for the linear algebra implementations
 *
 * Copyright (C) 2004-2009 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef LINWRP_DEF
#define LINWRP_DEF

#include <prime.h>
#include <tcl.h>

typedef struct {
    Tcl_Interp *ip;
    const char *progvar;
    int         pmsk;
} progressInfo;

/* I'm too tired right now, to put comments into this code! */

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
    void *(*orthoFunc)(primeInfo *pi, void *inp, void *urb, progressInfo *prg);
    void *(*liftFunc)(primeInfo *pi, void *inp, void *lft, progressInfo *prg);
    void (*quotFunc)(primeInfo *pi, void *ker, void *im, progressInfo *prg);
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

void *createStdMatrixCopy(matrixType *mt, void *mat);
void *createStdVectorCopy(vectorType *vt, void *vec);

int LAVadd(vectorType **vt1, void **vec1, 
           vectorType *vt2, void *vec2, int scale, int mod);

int LAMadd(matrixType **vt1, void **vec1, 
           matrixType *vt2, void *vec2, int scale, int mod);

int vectorIsZero(vectorType *vt, void *vdat);
int matrixIsZero(matrixType *mt, void *mdat);

#endif

