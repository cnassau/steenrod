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

#ifndef LINWRP_DEF
#define LINWRP_DEF

#include <prime.h>
#include <tcl.h>

typedef struct {
    Tcl_Interp *ip;
    const char *varname;
    int        numSteps;
} progressInfo;

/* I'm too (fucking) tired right now, to put comments into this code! */

typedef struct {
    int  (*getEntry)(void *vec, int idx, int *val);
 
    int  (*setEntry)(void *vec, int idx, int val);

    int  (*getLength)(void *vec);
    
    void *(*createVector)(int cols);

    void *(*createCopy)(void *vec);

    void (*destroyVector)(void *vec);
} vectorType;

typedef struct {
    int  (*getEntry)(void *mat, int row, int col, int *val); 

    int  (*setEntry)(void *mat, int row, int col, int val);

    void (*getDimensions)(void *mat, int *row, int *col);

    void *(*createMatrix)(int row, int col);

    void *(*createCopy)(void *mat);

    void (*destroyMatrix)(void *mat);

    void (*clearMatrix)(void *mat);

    void (*unitMatrix)(void *mat);

    void *(*orthoFunc)(primeInfo *pi, void *inp, progressInfo *prg);

    void *(*liftFunc)(primeInfo *pi, void *inp, void *lft, progressInfo *prg);

    void (*quotFunc)(primeInfo *pi, void *ker, void *im, progressInfo *prg);
} matrixType;

#ifndef LINWRPC
extern matrixType stdMatrixType;
extern vectorType stdVectorType;
#endif

#define stdmatrix (&(stdMatrixType))
#define stdvector (&(stdVectorType))

void *createStdMatrixCopy(matrixType *mt, void *mat);
void *createStdVectorCopy(vectorType *vt, void *vec);

#endif

