/*
 * The basic linear algebra stuff
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

#include "linalg.h"
#include <stdlib.h>
#include <string.h>

vector * vector_create(int size) {
    vector *v = mallox(sizeof(vector));
    if (NULL == v) return NULL;
    v->data = mallox(sizeof(cint) * size);
    if (NULL == v->data) { freex(v); return NULL; }
    v->num = size;
    return v;
}

void vector_dispose(vector *v) { freex(v->data); }

void vector_clear(vector *v) {
    int k; cint *dat;
    for (k=v->num, dat=v->data; k--;) *dat++ = 0;
}

int vector_iszero(vector *v) {
    int k; cint *dat;
    for (k=v->num, dat=v->data; k--;)
        if (*dat++) return 0;
    return 1;
}

void vector_copy(vector *v, vector *w) {
    int k; cint *dat, *sdat;
    for (k=v->num, dat=v->data, sdat=w->data; k--;) *dat++ = *sdat++;
}

void vector_add(vector *dst, vector *src, cint coeff, cint prime) {
    int k; cint *ddat, *sdat;
    for (k=dst->num, ddat=dst->data, sdat=src->data; k--; ddat++, sdat++) {
    *ddat += CINTMULT(coeff, *sdat, prime);
        *ddat %= prime;
    }
}

void vector_add_entry(vector *dst, int off, cint dat, cint prime) {
    dst->data[off] += dat;
    dst->data[off] %= prime;
}

cint vector_get_entry(vector *src, int off) {
    return src->data[off];
}

void vector_randomize(vector *v, cint prime) {
    int k; cint *dat;
    for (k=v->num, dat=v->data; k--;)
        *dat++ = random_cint(prime);
}

/* For some "reason" that I've forgotten, we try to adjust nomcols
 * so that rows lie on 8-byte boundaries. */
matrix *matrix_create(int rows, int cols) {
    matrix *res = (matrix *) mallox(sizeof(matrix));
    if (NULL == res) return NULL;
    res->nomcols = res->cols = cols;
    res->rows = rows;
    res->nomcols *= sizeof(cint); /* ... now bytes .. */
    res->nomcols += 7;
    res->nomcols /= 8;
    res->nomcols *= 8;
    res->nomcols /= sizeof(cint); /* ... now cint again  .. */
    res->data = (cint *) mallox(sizeof(cint) * res->nomcols * res->rows);
    if (NULL == res->data) {
        freex(res); return NULL;
    }
    return res;
}

void matrix_destroy(matrix *mat) {
    freex(mat->data); freex(mat);
}

void matrix_clear(matrix *mat) {
    memset(mat->data, 0, sizeof(cint) * mat->nomcols * mat->rows);
}

void matrix_unit(matrix *mat) {
    int i, off; cint *aux;
    matrix_clear(mat);
#if 0
    assert (mat->rows == mat->cols);
#endif
    off = mat->nomcols + 1;
    for (aux=mat->data,i=0;i<mat->rows;i++,aux+=off) *aux = 1;
}

vector *matrix_get_row(matrix *m, int r) {
    vector *res = mallox(sizeof(vector)) ;
    if (NULL == res) return NULL;
    res->num = m->cols;
    res->data = m->data + (m->nomcols * r);
    return res;
}

void matrix_collect(matrix *m, int r) {
    vector dst, src;

    dst.data = m->data + m->nomcols * m->rows;
    src.data = m->data + m->nomcols * r;

    dst.num = src.num = m->cols;

    if (dst.data != src.data) vector_copy(&dst, &src);

    m->rows++;
}

/* try to shrink or enlarge m; move if necessary */
int matrix_resize(matrix *m, int newrows) {
    int nsz = m->nomcols * sizeof(cint) * newrows;
    cint *nw;
    nw = reallox(m->data, nsz);
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
    vec.data = m->data + k * m->nomcols * sizeof(cint);
    vec.num = m->cols;
    vector_randomize(&vec, prime);
    }
}
