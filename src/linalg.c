/*
 * The basic linear algebra stuff
 *
 * Copyright (C) 2004 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "linalg.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef USESSE2
#  include "ssedefs.h"
#endif

vector * vector_create(int size) {
    vector *v = mallox(sizeof(vector));
    if (NULL == v) return NULL;
    v->blocks = BLOCKS_REQUIRED(size);
    v->data = (BLOCKTYPE *) mallox(sizeof(BLOCKTYPE) * v->blocks);
    if (NULL == v->data) { freex(v); return NULL; }
    v->num = size;
    return v;
}

void vector_dispose(vector *v) { freex(v->data); }

void vector_clear(vector *v) {
    memset(v->data,0,v->blocks * sizeof(BLOCKTYPE));
}

#ifdef USESSE2
#  define COMPAREBLOCKS(x,y) \
    (_mm_movemask_epi8(_mm_cmpgt_epi8((x),(y))))
int COMPAREBLOCKSDBG(__m128i x,__m128i y) {
    __m128i u = _mm_cmpgt_epi8(x,y);
    int r = _mm_movemask_epi8(u);
    PRINTEPI8(x);PRINTEPI8(y);PRINTEPI8(u); printf("movemask=%x\n",r);
    return r;
}
#else
#  define COMPAREBLOCKS(x,y) ((x) == (y))
#endif

inline
int BLOCKSAREZERO(BLOCKTYPE *dat, int numblocks) {
#ifdef USESSE2
    const __m128i zero = _mm_setzero_si128();
#else
    const BLOCKTYPE zero = 0;
#endif
    while (numblocks--) 
        if(COMPAREBLOCKS(*dat++,zero))
            return 0;
    return 1;
}

int vector_iszero(vector *v) {
    return BLOCKSAREZERO(v->data,v->blocks);
}


int matrix_iszero(matrix *m) {
    return BLOCKSAREZERO(m->data,m->nomcols * m->rows);
}

void vector_copy(vector *v, vector *w) {
    int k; BLOCKTYPE *dat, *sdat;
    for (k=v->blocks, dat=v->data, sdat=w->data; k--;) *dat++ = *sdat++;
}

void vector_add(vector *dst, vector *src, cint coeff, cint prime) {
#ifdef USESSE2
    add_blocks(dst->data,src->data,dst->blocks,coeff,prime);
#else
    int k; BLOCKTYPE *ddat, *sdat;
    for (k=dst->blocks, ddat=dst->data, sdat=src->data; k--; ddat++, sdat++) {
        *ddat += CINTMULT(coeff, *sdat, prime);
        if (prime) *ddat %= prime;
    }
#endif
}

void vector_add_entry(vector *dst, int off, cint dat, cint prime) {
#ifdef USESSE2
    char c = extract_entry(dst->data[off/16],off&15);
    c += dat; c %= prime; 
    set_entry(&(dst->data[off/16]),off&15,c);
#else
    dst->data[off] += dat;
    if (prime) dst->data[off] %= prime;
#endif
}

cint vector_get_entry(vector *src, int off) {
#ifdef USESSE2
    return extract_entry(src->data[off/16],off&15);
#else
    return src->data[off];
#endif
}

void vector_set_entry(vector *src, int off, cint val) {
#ifdef USESSE2
    set_entry(&(src->data[off/16]),off&15,val);
#else
    src->data[off] = val;
#endif
}
void vector_reduce(vector *v, cint prime) {
#ifdef USESSE2
    reduce_blocks(v->data,v->blocks,prime);
#else
    int i;
    for (i=v->num;i--;)
        v->data[i] %= prime;
#endif
}

#ifdef USESSE2
#  define RANDOM_BLOCK(x) _mm_set_epi8 (random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime),\
                                        random_cint(prime))
#else
#  define RANDOM_BLOCK(x) random_cint(prime)
#endif

void vector_randomize(vector *v, cint prime) {
    int k; BLOCKTYPE *dat;
    for (k=v->blocks, dat=v->data; k--;)
        *dat++ = RANDOM_BLOCK(prime);
}

matrix *matrix_create(int rows, int cols) {
    matrix *res = (matrix *) mallox(sizeof(matrix));
    if (NULL == res) return NULL;
    res->cols = cols;
    res->rows = rows;
    res->nomcols = BLOCKS_REQUIRED(cols);
#ifndef USESSE2
    /* make this a multiple of 8, might help to speed things up */
    res->nomcols = (7 + res->nomcols)/8; res->nomcols *= 8;
#endif
    res->data = (BLOCKTYPE *) 
        mallox(sizeof(BLOCKTYPE) * res->nomcols * res->rows);
    if (NULL == res->data) {
        freex(res); return NULL;
    }
    return res;
}

void matrix_destroy(matrix *mat) {
    freex(mat->data); freex(mat);
}

matrix *matrix_copy(matrix *mat) {
    matrix *res = matrix_create(mat->rows, mat->cols);
    if (NULL == res) return NULL;
    memcpy(res->data, mat->data, sizeof(BLOCKTYPE) * mat->nomcols * mat->rows);
    return res;
}

void matrix_clear(matrix *mat) {
    memset(mat->data, 0, sizeof(BLOCKTYPE) * mat->nomcols * mat->rows);
}

void matrix_unit(matrix *mat) {
#ifdef USESSE2
    int i;
    matrix_clear(mat);
    for (i=mat->rows;i--;)
        matrix_set_entry(mat,i,i,1);
#else
    int i, off; cint *aux;
    matrix_clear(mat);
    off = mat->nomcols + 1;
    for (aux=mat->data,i=0;i<mat->rows;i++,aux+=off) *aux = 1;
#endif
}

void make_matrix_row(vector *v, matrix *m, int r) {
    v->num = m->cols;
    v->blocks = m->nomcols;
    v->data = m->data + (m->nomcols * r);
}

vector *matrix_get_row(matrix *m, int r) {
    vector *res = mallox(sizeof(vector)) ;
    if (NULL == res) return NULL;
    make_matrix_row(res, m, r);
    return res;
}

cint matrix_get_entry(matrix *m, int r, int c) {
#ifdef USESSE2
    return extract_entry(m->data[m->nomcols * r + c/16],c & 15);
#else
    cint *rowptr = m->data + (m->nomcols * r);
    return rowptr[c];
#endif
}

void matrix_set_entry(matrix *m, int r, int c, cint val) {
#ifdef USESSE2
    set_entry(&(m->data[m->nomcols * r + c/16]),c & 15,val);
#else
    cint *rowptr = m->data + (m->nomcols * r);
    rowptr[c] = val; 
#endif
}

void matrix_add(matrix *dst, matrix *src, cint coeff, cint prime) {
#ifdef USESSE2
    if ((dst->rows != src->rows) || (dst->cols != src->cols)) return;
    add_blocks(dst->data,src->data,dst->nomcols * dst->rows,coeff,prime);
#else
    int i;
    vector v1, v2;
    if ((dst->rows != src->rows) || (dst->cols != src->cols)) return;
    for (i=dst->rows; i--;) {
        make_matrix_row(&v1, dst, i);
        make_matrix_row(&v2, src, i);
        vector_add(&v1, &v2, coeff, prime);
    }
#endif 
}

void matrix_collect(matrix *m, int r) {
    if (r != m->rows) {
        vector dst, src;
        make_matrix_row(&dst, m, m->rows);
        make_matrix_row(&src, m, r);
        vector_copy(&dst, &src);
    }

    m->rows++;
}

/* try to shrink or enlarge m; move if necessary */
int matrix_resize(matrix *m, int newrows) {
    int nsz = m->nomcols * sizeof(BLOCKTYPE) * newrows;
    BLOCKTYPE *nw;
    nw = (BLOCKTYPE *) reallox(m->data, nsz);
    if ((NULL!=nw) || (0 == nsz)) {
        m->data = nw; m->rows = newrows;
        return 0;
    }
    return 1;
}

void matrix_randomize(matrix *m, cint prime) {
    vector vec;
    int k;
    for (k=0;k<m->rows;k++) {
        make_matrix_row(&vec, m, k);
        vector_randomize(&vec, prime);
    }
}

void matrix_reduce(matrix *m, cint prime) {
#ifdef USESSE2
    reduce_blocks(m->data,m->nomcols * m->rows,prime);
#else
    cint *rowptr; int i,j;
    for (j=m->rows;j--;) {
        rowptr = m->data + j* m->nomcols;
        for (i=m->cols;i--;)
            rowptr[i] %= prime;
    }
#endif
}
