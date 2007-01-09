/*
 * Optimized linear algebra stuff for the prime 2
 *
 * Copyright (C) 2005-2007 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define LINWRPC2

#include <string.h>
#include "linwrp.h"

#define BITSPERINT (sizeof(int) * 8)
#define IPROCO(cols) ((BITSPERINT-1+(cols))/BITSPERINT)

typedef struct {
    int *data;
    int size, ints;
} vec2;

int stdVGetEntry2(void *vec, int idx, int *val) {
    vec2 *v = (vec2 *) vec;
    int off = idx/BITSPERINT, msk = 1 << (idx % BITSPERINT);
    if (idx >= (v->size)) return FAILIMPOSSIBLE;
    *val = (msk & v->data[off]) ? 1 : 0;
    return SUCCESS;
}

int stdVSetEntry2(void *vec, int idx, int val) {
    vec2 *v = (vec2 *) vec;
    int off = idx/BITSPERINT, msk = 1 << (idx % BITSPERINT);
    if (0 == (val & 0x1)) return SUCCESS;
    if (idx >= (v->size)) return FAILIMPOSSIBLE;
    if (val & 0x1) {
        v->data[off] |= msk;
    } else {
        v->data[off] &= ~msk;
    }
    return SUCCESS;
}

int stdVGetLength2(void *vec) {
    vec2 *v = (vec2 *) vec;
    return v->size;
}

void *stdVCreateVector2(int cols) {
    vec2 *v = (vec2 *) mallox(sizeof(vec2));
    if (NULL != v) {
        v->size = cols;
        v->ints = IPROCO(cols);
        if (NULL == (v->data = callox(v->ints,sizeof(int)))) {
            freex(v);
            return NULL;
        }
    }
    return v;
}

void *stdVCreateCopy2(void *vec) {
    vec2 *v = (vec2 *) vec;
    vec2 *w = (vec2 *) mallox(sizeof(vec2));
    if (NULL != w) {
        w->size = v->size;
        w->ints = v->ints;
        if (NULL == (w->data = mallox(v->ints))) {
            freex(w);
            return NULL;
        }
        memcpy(w,v,sizeof(int) * w->ints);
    }
    return w;
}

void stdVDestroyVector2(void *vec) {
    vec2 *v2 = (vec2 *) vec;
    freex(v2->data);
    freex(vec);
}

int stdVReduce2(void *vec, int prime) {
    return SUCCESS;
}

vectorType stdVectorType2 = {
    .name          = "stdvector2",
    .getEntry      = &stdVGetEntry2,
    .setEntry      = &stdVSetEntry2,
    .getLength     = &stdVGetLength2,
    .createVector  = &stdVCreateVector2,
    .createCopy    = &stdVCreateCopy2,
    .destroyVector = &stdVDestroyVector2,
    .reduce        = &stdVReduce2,
    .add           = NULL
};

typedef struct {
    int *data;
    int rows, cols;
    int ipr; /* ints per row */
} mat2;

int stdGetEntry2(void *m, int row, int col, int *val) {
    mat2 *mat = (mat2 *) m;
    int off = col/BITSPERINT, msk = 1 << (col % BITSPERINT);
    if ((row >= mat->rows) || (col >= mat->cols))
        return FAILIMPOSSIBLE;
    *val = (mat->data[row*(mat->ipr)+off] & msk) ? 1 : 0;
    return SUCCESS;
}

int stdSetEntry2(void *m, int row, int col, int val) {
    mat2 *mat = (mat2 *) m;
    int off = col/BITSPERINT, msk = 1 << (col % BITSPERINT);
    if ((row >= mat->rows) || (col >= mat->cols))
        return FAILIMPOSSIBLE;
    off += row*(mat->ipr);
    if (val & 0x1) {
        mat->data[off] |= msk;
    } else {
        mat->data[off] &= ~msk;
    }
    return SUCCESS;
}

int stdAddToEntry2(void *m, int row, int col, int val, int mod) {
    mat2 *mat = (mat2 *) m;
    int off = col/BITSPERINT, msk = 1 << (col % BITSPERINT);
    if ((row >= mat->rows) || (col >= mat->cols))
        return FAILIMPOSSIBLE;
    if (0 == (val & 0x1)) return SUCCESS;  
    off += row*(mat->ipr);
    mat->data[off] ^= msk;
    return SUCCESS;  
}

void stdGetDimensions2(void *m, int *row, int *col) {
    mat2 *mat = (mat2 *) m;
    *row = mat->rows; *col = mat->cols;
}

void *stdCreateMatrix2(int row, int col) {
    mat2 *m = (mat2 *) mallox(sizeof(mat2));
    if (NULL != m) {
        m->rows = row; m->cols = col; m->ipr = IPROCO(col);
        if (NULL == (m->data = callox(row * m->ipr,sizeof(int)))) {
            freex(m);
            return NULL;
        }
    }
    return m;
}

void *stdCreateMCopy2(void *mat) {
    mat2 *m = (mat2 *) mat;
    mat2 *res = stdCreateMatrix2(m->rows, m->cols);
    if (NULL != res) {
        memcpy(res->data,m->data,m->ipr * m->rows * sizeof(int));
    }
    return res;
}

void stdDestroyMatrix2(void *mat) {
    mat2 *m = (mat2 *) mat;
    freex(m->data);
    freex(m);
}

void stdClearMatrix2(void *mat) {
    mat2 *m = (mat2 *) mat;
    memset(m->data,0,sizeof(int) * m->ipr * m->rows);
}

void stdUnitMatrix2(void *mat) {
    mat2 *m = (mat2 *) mat;
    int i,r = m->rows < m->cols ? m->rows : m->cols;
    stdClearMatrix2(mat);
    for (i=0;i<r;i++)
        stdSetEntry2(mat,i,i,1);
}

int stdReduceMatrix2(void *mat, int prime) {
    return SUCCESS;
}

// forward declarations
mat2 *matrix_ortho2(mat2 *inp, mat2 **urb, Tcl_Interp *ip, const char *progvar, int pmsk);
mat2 *matrix_lift2(mat2 *inp, mat2 *lft, Tcl_Interp *ip, const char *progvar, int pmsk);
int matrix_quotient2(mat2 *ker, mat2 *im, Tcl_Interp *ip, const char *progvar, int pmsk);

void *stdOrthoFunc2(primeInfo *pi, void *inp, void *urb, progressInfo *prg) {
    mat2 *ker;
    Tcl_Interp *ip = NULL; 
    const char *progvar = NULL;
    int pmsk = 0;
    if (NULL != prg) { 
        ip = prg->ip; progvar = prg->progvar; pmsk = prg->pmsk; 
    }
    ker = matrix_ortho2((mat2 *) inp, (mat2 **) urb, ip, progvar, pmsk);
    return ker;
}

void *stdLiftFunc2(primeInfo *pi, void *inp, void *lft, progressInfo *prg) {
    mat2 *res;
    Tcl_Interp *ip = NULL; 
    const char *progvar = NULL;
    int pmsk = 0;
    if (NULL != prg) { ip = prg->ip; progvar = prg->progvar; pmsk = prg->pmsk; }
    res = matrix_lift2((mat2 *) inp, lft, ip, progvar, pmsk);
    return res;
}

void stdQuotFunc2(primeInfo *pi, void *ker, void *im, progressInfo *prg) {
    Tcl_Interp *ip = NULL; 
    const char *progvar = NULL;
    int pmsk = 0;
    if (NULL != prg) { ip = prg->ip; progvar = prg->progvar; pmsk = prg->pmsk; }
    matrix_quotient2((mat2 *) ker, (mat2 *) im, ip, progvar, pmsk);
}

void vector_add2(int *a, int *b, int nint) {
    while (nint--) *a++ ^= *b++;
}

int stdAddMatrix2(void *m1, void *m2, int scale, int mod) {
    mat2 *x1 = (mat2 *) m1;
    mat2 *x2 = (mat2 *) m2;
    if (0 == (0x1 & scale)) return SUCCESS;
    if ((x1->rows != x2->rows) || (x1->cols != x2->cols))
        return FAILIMPOSSIBLE;
    vector_add2(x1->data,x2->data,x1->ipr * x1->rows);
    return SUCCESS;
}

void *stdShrinkMatrix2(void *mat, int *indices, int numind) {
    mat2 *m = (mat2 *) mat;
    mat2 *res = stdCreateMatrix2(numind,m->cols);
    if (NULL != res) {
        int i, ipr = m->ipr;
        for (i=0;i<numind;i++) {
            memcpy(res->data+i*ipr,m->data + *indices++ * ipr,ipr*sizeof(int));
        }
    }
    return res;
}

matrixType stdMatrixType2 = {
    .name          = "stdmatrix2",
    .getEntry      = stdGetEntry2,
    .setEntry      = stdSetEntry2,
    .addToEntry    = stdAddToEntry2,
    .getDimensions = stdGetDimensions2,
    .createMatrix  = stdCreateMatrix2,
    .createCopy    = stdCreateMCopy2,
    .destroyMatrix = stdDestroyMatrix2,
    .clearMatrix   = stdClearMatrix2,
    .unitMatrix    = stdUnitMatrix2,
    .shrinkRows    = stdShrinkMatrix2,
    .reduce        = stdReduceMatrix2,
    .add           = stdAddMatrix2,
    .orthoFunc     = stdOrthoFunc2,
    .liftFunc      = stdLiftFunc2,
    .quotFunc      = stdQuotFunc2
};


#define  PROGVARINIT     \
    double perc = 0;     \
    if (NULL != progvar) Tcl_LinkVar(ip, progvar, (char *) &perc, TCL_LINK_DOUBLE);

#define PROGVARSET(val)  \
    if (NULL != progvar) Tcl_UpdateLinkedVar(ip, progvar); \
    if (LINALG_INTERRUPT_VARIABLE) goto done;

#define PROGVARDONE \
    if (NULL != progvar) Tcl_UnlinkVar(ip, progvar);

#define LINALG_INTERRUPT_VARIABLE 0

void matrix_collect_ext2(mat2 *dst, mat2 *src, int i) {
    int off1 = dst->rows * dst->ipr;
    int off2 = i * src->ipr;
    memcpy(dst->data + off1, src->data + off2, sizeof(int) * src->ipr);
    dst->rows++;
}

void matrix_collect2(mat2 *m, int i) {
    int off1 = m->rows * m->ipr;
    int off2 = i * m->ipr;
    memcpy(m->data + off1, m->data + off2, sizeof(int) * m->ipr);
    m->rows++;
}

int matrix_resize2(mat2 *m, int newrows) {
    int nsz = m->ipr * sizeof(int) * newrows;
    int *nw;
    nw = reallox(m->data, nsz);
    m->rows = newrows;
    if ((NULL!=nw) || (0 == nsz)) {
        m->data = nw; 
        return 0;
    }
    return 1;
}

// forward declarations
mat2 *matrix_ortho2(mat2 *inp, mat2 **urb, Tcl_Interp *ip, 
                    const char *progvar, int pmsk) {
    int i,j,cols, spr, uspr;
    int failure = 1;     /* pessimistic, eh? */
    int *v1,*v2,*v3,*v4;
    int *aux;

    mat2 m1, m2, m3;
    mat2 *un, *oth = NULL;

    PROGVARINIT ;

    un = (mat2 *) stdCreateMatrix2(inp->rows, inp->rows);
    if (NULL == un) return NULL;
    stdUnitMatrix2(un);

    if (urb) {
        oth = (*urb = stdCreateMatrix2(inp->rows, inp->rows));
        if (NULL == oth) {
            stdDestroyMatrix2(un);
            return NULL;
        }
    }

    cols = inp->cols;

    spr = inp->ipr; /* ints per row */
    uspr = un->ipr;

    /* make m1, m2 empty copies of inp, resp. un */
    m1.ipr = inp->ipr;
    m1.cols = inp->cols; m1.data = inp->data; m1.rows = 0;
    m2.ipr = un->ipr;
    m2.cols = un->cols;  m2.data = un->data;  m2.rows = 0;
    if (oth != NULL) {
        m3.cols = oth->cols;
        m3.data = oth->data;  m3.ipr = oth->ipr;  m3.rows = 0;
    }

    for (v1=inp->data, i=0; i<inp->rows; i++, v1+=spr) {
      
        if ((NULL != progvar) && (0==(i&pmsk))) {
            perc = i; perc /= inp->rows;
            perc = 1-perc; perc *= perc; perc = 1-perc;
            PROGVARSET(perc);
        }

        /* find pivot for this row */
        for (aux=v1, j=spr; j; aux++, j--)
            if (0 != *aux) break;

        if (0 == j) {
            /* row is zero */
            matrix_collect2(&m2, i); /* collect kernel vector */
        } else {
            int pivmsk = *aux; pivmsk ^= pivmsk & (pivmsk-1);
            matrix_collect2(&m1, i); /* collect image vector */
            if (NULL != oth) {
                matrix_collect_ext2(&m3, &m2, i);
            }
             /* go through all other rows and normalize */
            v2 = v1 + spr; aux += spr;
            v3 = un->data + i * uspr;
            v4 = v3 + uspr;
            for (j=i+1; j<inp->rows; j++, v2+=spr, v4+=uspr, aux+=spr) {
                if (0 != (*aux & pivmsk)) {
                    vector_add2(v4, v3, uspr);
                    vector_add2(v2, v1, spr);
                }
            }
        }
    }

    failure = 0;

 done:
    if (failure) {
        stdDestroyMatrix2(un);
        un = NULL;
    } else {
        if (TCL_OK != matrix_resize2(inp, m1.rows)) return NULL;
        if (TCL_OK != matrix_resize2(un, m2.rows)) return NULL;
        if ((NULL != oth) && (TCL_OK != matrix_resize2(oth, m3.rows))) return NULL;
    }

    PROGVARDONE ;

    return un;
}

mat2 *matrix_lift2(mat2 *inp, mat2 *lft, Tcl_Interp *ip, const char *progvar, int pmsk) {
    
    int i,j,cols, spr, uspr;
    int failure=1;
    int *v1,*v2,*v3,*v4;
    int *aux;

    mat2 *un, *res ;

    PROGVARINIT ;

    un = (mat2 *) stdCreateMatrix2(inp->rows, inp->rows);
    if (NULL == un) return NULL;
    stdUnitMatrix2(un);

    res = (mat2 *) stdCreateMatrix2(lft->rows, inp->rows);
    if (NULL == res) {
        stdDestroyMatrix2(un);
        return NULL;
    }

    cols = inp->cols;

    spr = inp->ipr; /* ints per row */
    uspr = un->ipr;

    for (v1=inp->data, i=0; i<inp->rows; i++, v1+=spr) {
        int pos;
        if ((NULL != progvar) && (0==(i&pmsk))) {
            perc = i; perc /= inp->rows;
            perc = 1-perc; perc *= perc; perc = 1-perc;
            PROGVARSET(perc);
        }
        /* find pivot for this row */
        for (aux=v1, j=spr; j; aux++, j--)
            if (0 != *aux) break;
        if (0 == j) {
            /* row is zero */
        } else {
            int pivmsk = *aux; pivmsk ^= pivmsk & (pivmsk-1);
            pos = aux - v1;
            /* go through all other rows and normalize */
            v2 = v1 + spr; aux += spr;
            v3 = un->data + i * uspr;
            v4 = v3 + uspr;
            for (j=i+1; j<inp->rows;
                 j++, v2+=spr, v4+=uspr, aux+=spr) {
                if (0 != (*aux & pivmsk)) {
                    vector_add2(v4, v3, uspr);
                    vector_add2(v2, v1, spr);
                }
            }
            /* reduce vectors in lft in the same way */
            v2 = lft->data; v4 = res->data; aux = v2 + pos;
            for (j=0; j<lft->rows; j++, v2+=spr, v4+=uspr, aux+=spr) {
                if (0 != (*aux & pivmsk)) {
                    vector_add2(v4, v3, uspr);
                    vector_add2(v2, v1, spr);
                }
            }
        }
    }

    failure = 0;
 done:

    PROGVARDONE ;

    stdDestroyMatrix2(un);

    if (failure) {
        stdDestroyMatrix2(res);
        res = NULL;
    }

    return res;
}

int matrix_quotient2(mat2 *ker, mat2 *im, Tcl_Interp *ip, const char *progvar, int pmsk) {
    int i,j,cols, spr;
    int failure = 1;
    int *v1,*v2;
    int *aux;
    mat2 m1;

    PROGVARINIT ;

    /* m1 is used to collect quotient vectors from ker */
    m1.data = ker->data; m1.ipr = ker->ipr;
    m1.cols = ker->cols; m1.rows= 0;

    if (0 == im->rows) im->cols = ker->cols;

    cols = im->cols;

#if 0
    ASSERT(im->cols == ker->cols);
#endif

    spr = im->ipr; /* ints per row */

    for (v1=im->data, i=0; i<im->rows; i++, v1+=spr) {
        int pos;
        if ((NULL != progvar) && (0==(i&pmsk))) {
            perc = i; perc /= im->rows;
            perc = 1-perc; perc *= perc; perc = 1-perc;
            PROGVARSET(perc);
        }
        /* find pivot for this row */
        for (aux=v1, j=spr; j; aux++, j--)
            if (0 != *aux) break;
        if (0 == j) {  
            /* row is zero */ ASSERT(0=="row shouldn't be zero!"); 
        } else {
            int pivmsk = *aux; pivmsk ^= pivmsk & (pivmsk-1);
            pos = aux - v1;
            /* reduce vectors in ker in the usual way */
            v2 = ker->data; aux = v2 + pos;
            for (j=0; j<ker->rows; j++, v2+=spr,  aux+=spr) {
                if (0 != (*aux & pivmsk)) {
                    vector_add2(v2, v1, spr);
                }
            }
        }
    }

    /* now reduce ker and collect results */

    PROGVARSET(-1.0); /* -1 to indicate beginning of last phase */

    for (v1=ker->data, i=0; i<ker->rows; i++, v1+=spr) {
        int pos;
        /* find pivot for this row */
        for (aux=v1, j=spr; j; aux++, j--)
            if (0 != *aux) break;
        if (0 == j) {
            /* row is zero */
        } else {
            int pivmsk = *aux; pivmsk ^= pivmsk & (pivmsk-1);
            pos = aux - v1;
            matrix_collect2(&m1, i); /* collect this row */
            /* reduce other vectors in ker */
            v2 = v1 + spr; aux = v2 + pos;
            for (j=i+1; j<ker->rows; j++, v2+=spr,  aux+=spr) {
                if (0 != (*aux & pivmsk)) {
                    vector_add2(v2, v1, spr);
                }
            }
        }
    }

    PROGVARDONE ;

    failure = 0;

 done:;

    if (TCL_OK != matrix_resize2(ker, m1.rows)) return TCL_ERROR;

    return SUCCESS;
}

