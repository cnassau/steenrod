/*
 * Advanced linear algebra - sse2 optimized code
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

#include "adlin.h"
#include "ssedefs.h"
#include <tcl.h>
#include "linalg.h"

#define  PROGVARINIT     \
    double perc = 0;     \
    if (NULL != progvar) Tcl_LinkVar(ip, progvar, (char *) &perc, TCL_LINK_DOUBLE);

#define PROGVARSET(val)  \
    if (NULL != progvar) Tcl_UpdateLinkedVar(ip, progvar); \
    if (*LINALG_INTERRUPT_VARIABLE) goto done;

#define PROGVARDONE \
    if (NULL != progvar) Tcl_UnlinkVar(ip, progvar);

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

/* orthonormalize the input matrix, return basis of kernel */
matrix *matrix_ortho(primeInfo *pi, matrix *inp, matrix **urb,
                     Tcl_Interp *ip, const char *progvar, int pmsk,
		     int *LINALG_INTERRUPT_VARIABLE) {
    int i,j,cols, spr, uspr;
    int failure = 1;     /* pessimistic, eh? */
    vector v1,v2,v3,v4;
    __m128i *aux;
    int prime = pi->prime;

    matrix m1, m2, m3;
    matrix *un, *oth = NULL;

    PROGVARINIT ;

    un = matrix_create(inp->rows, inp->rows);
    if (NULL == un) return NULL;
    matrix_unit(un);

    if (urb) {
        oth = (*urb = matrix_create(inp->rows, inp->rows));
        if (NULL == oth) {
            matrix_destroy(un);
            return NULL;
        }
    }

    cols = inp->cols;

    make_matrix_row(&v1,inp,0);
    make_matrix_row(&v2,inp,0);
    make_matrix_row(&v3,un,0);
    make_matrix_row(&v4,un,0);

    spr = inp->nomcols; /* blocks per row */
    uspr = un->nomcols;

    /* make m1, m2 empty copies of inp, resp. un */

    m1.nomcols = inp->nomcols; 
    m1.cols = inp->cols; m1.data = inp->data; m1.rows = 0;
    m2.nomcols = un->nomcols;  
    m2.cols = un->cols;  m2.data = un->data;  m2.rows = 0;
    if (oth != NULL) {
        m3.nomcols = oth->nomcols;  
        m3.cols = oth->cols;  m3.data = oth->data;  m3.rows = 0;
    }

    for (v1.data=inp->data, i=0; i<inp->rows; i++, v1.data+=spr) {
        int coeff, bmsk=0;
        const __m128i zero = _mm_setzero_si128();
        if ((NULL != progvar) && (0==(i&pmsk))) {
            perc = i; perc /= inp->rows;
            perc = 1-perc; perc *= perc; perc = 1-perc;
            PROGVARSET(perc);
        }
        /* find pivot for this row */
        for (aux=v1.data, j=spr; j; aux++, j--) {
            bmsk = 0xffff ^ _mm_movemask_epi8(_mm_cmpeq_epi8(*aux, zero));
            if (bmsk) break;
        }

        if (0 == bmsk) {
            /* row is zero */
            matrix_collect(&m2, i); /* collect kernel vector */
        } else {
            int idx = 0;
            matrix_collect(&m1, i); /* collect image vector */
            if (NULL != oth) {
                matrix_collect_ext(&m3, &m2, i);
            }

            // find index of first non-zero entry
            while (0 == (bmsk & 1)) { idx++; bmsk >>= 1; }
            coeff = extract_entry(*aux,idx) % prime;
            if (coeff<0) coeff += prime;
            coeff = pi->inverse[(unsigned) coeff]; 
            coeff = prime-coeff; 

            /* go through all other rows and normalize */
            v2.data = v1.data + spr; aux += spr;
            v3.data = un->data + i * uspr;
            v4.data = v3.data + uspr;
            for (j=i+1; j<inp->rows; 
                 j++, v2.data+=spr, v4.data+=uspr, aux+=spr) {
                unsigned val = extract_entry(*aux,idx) % prime; 
                if (val) {
                    int ncoeff = CINTMULT(val,coeff,prime);
                    vector_add(&v4, &v3, ncoeff, prime);
                    vector_add(&v2, &v1, ncoeff, prime);
                }
            }
        }
    }

    failure = 0; 

 done: 
    if (failure) {
        matrix_destroy(un);
        un = NULL;
    } else {
        if (TCL_OK != matrix_resize(inp, m1.rows)) return NULL;
        if (TCL_OK != matrix_resize(un, m2.rows)) return NULL;
        if ((NULL != oth) && (TCL_OK != matrix_resize(oth, m3.rows))) return NULL;
    }

    PROGVARDONE ; 

    return un;
}


/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

matrix *matrix_lift(primeInfo *pi, matrix *inp, matrix *lft, 
                    Tcl_Interp *ip, const char *progvar, int pmsk,
		    int *LINALG_INTERRUPT_VARIABLE) {

    int i,j,cols, spr, uspr;
    int failure=1;
    vector v1,v2,v3,v4;
    BLOCKTYPE *aux;
    int prime = pi->prime;
    matrix *un, *res ;

    PROGVARINIT ;

    un = matrix_create(inp->rows, inp->rows);
    if (NULL == un) return NULL;
    matrix_unit(un);

    res = matrix_create(lft->rows, inp->rows);
    if (NULL == res) {
        matrix_destroy(un);
        return NULL;
    }
    matrix_clear(res);

    cols = inp->cols;

    make_matrix_row(&v1,inp,0);
    make_matrix_row(&v2,inp,0);
    make_matrix_row(&v3,un,0);
    make_matrix_row(&v4,un,0);

    spr = inp->nomcols; /* blocks per row */
    uspr = un->nomcols;

    for (v1.data=inp->data, i=0; i<inp->rows; i++, v1.data+=spr) {
        int coeff, bmsk=0;
        __m128i zero = _mm_setzero_si128();
        
        if ((NULL != progvar) && (0==(i&pmsk))) {
            perc = i; perc /= inp->rows;
            perc = 1-perc; perc *= perc; perc = 1-perc;
            PROGVARSET(perc);
        }

        /* find pivot for this row */
        for (aux=v1.data, j=v1.blocks; j; aux++, j--) {
            bmsk = 0xffff ^ _mm_movemask_epi8(_mm_cmpeq_epi8(*aux, zero));
            if (bmsk) break;
        }

        if (0 == bmsk) {
            /* row is zero */
        } else {
            int idx = 0, entry, pos = aux - v1.data;
            unsigned int negcoeff;

            // find index of first non-zero entry
            while (0 == (bmsk & 1)) { idx++; bmsk >>= 1; }
            entry = extract_entry(*aux,idx) % prime;
            if (entry<0) entry += prime;
            negcoeff = coeff = pi->inverse[entry]; 
            coeff = prime-coeff;

            /* go through all other rows and normalize */
            v2.data = v1.data + spr; aux += spr;
            v3.data = un->data + i * uspr;
            v4.data = v3.data + uspr;
            for (j=i+1; j<inp->rows; 
                 j++, v2.data+=spr, v4.data+=uspr, aux+=spr) {
                unsigned val = extract_entry(*aux,idx) % prime; 
                if (val) {
                    cint cc = CINTMULT(val,coeff,prime);
                    vector_add(&v4, &v3, cc, prime);
                    vector_add(&v2, &v1, cc, prime);
                }
            }
            /* reduce vectors in lft in the same way */
            v2.data = lft->data; v4.data = res->data; aux = v2.data + pos;
            for (j=0; j<lft->rows; j++, v2.data+=spr, v4.data+=uspr, aux+=spr) {
                unsigned val = extract_entry(*aux,idx) % prime; 
                if (val) {
                    cint cc = CINTMULT(val,coeff,prime);
                    cint nc = prime-cc;
                    vector_add(&v4, &v3, nc, prime);
                    vector_add(&v2, &v1, cc, prime);
                }
            }
        }
    }

    failure = 0;
 done:

    PROGVARDONE ;

    matrix_destroy(un);

    if (failure) { 
        matrix_destroy(res);
        res = NULL; 
    }

    return res;
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

int matrix_quotient(primeInfo *pi, matrix *ker, matrix *im, 
                    Tcl_Interp *ip, const char *progvar, int pmsk,
		     int *LINALG_INTERRUPT_VARIABLE) {
    int i,j,cols, spr;
    cint prime = pi->prime;
    int failure = 1;
    vector v1,v2;
    BLOCKTYPE *aux;
    matrix m1;

    PROGVARINIT ;
 
    /* m1 is used to collect quotient vectors from ker */
    m1.data = ker->data; m1.nomcols = ker->nomcols; 
    m1.cols = ker->cols; m1.rows= 0;

    if (0 == im->rows) im->cols = ker->cols;

    cols = im->cols;
  
    make_matrix_row(&v1,im,0);
    make_matrix_row(&v2,im,0);

    spr = im->nomcols; /* blocks per row */

    for (v1.data=im->data, i=0; i<im->rows; i++, v1.data+=spr) {
        int coeff, bmsk=0;
        __m128i zero = _mm_setzero_si128();
        
        if ((NULL != progvar) && (0==(i&pmsk))) {
            perc = i; perc /= im->rows;
            perc = 1-perc; perc *= perc; perc = 1-perc;
            PROGVARSET(perc);
        }

        /* find pivot for this row */
        for (aux=v1.data, j=v1.blocks; j; aux++, j--) {
            bmsk = 0xffff ^ _mm_movemask_epi8(_mm_cmpeq_epi8(*aux, zero));
            if (bmsk) break;
        }

        if (0 == bmsk) { 
            /* row is zero */ 
        } else {
            int idx = 0, pos = aux-v1.data, entry;

            // find index of first non-zero entry
            while (0 == (bmsk & 1)) { idx++; bmsk >>= 1; }
            entry = extract_entry(*aux,idx) % prime;
            if (entry<0) entry += prime;
            coeff = pi->inverse[entry]; 
            coeff = prime-coeff;

            /* reduce vectors in ker in the usual way */
            v2.data = ker->data; aux = v2.data + pos;
            for (j=0; j<ker->rows; j++, v2.data+=spr,  aux+=spr) {
                unsigned val = extract_entry(*aux,idx) % prime; 
                if (val) {
                    int ncoeff = CINTMULT(val,coeff,prime);
                    vector_add(&v2, &v1, ncoeff, prime);
                }
            }
        }
    }

    /* now reduce ker and collect results */

    for (v1.data=ker->data, i=0; i<ker->rows; i++, v1.data+=spr) {
        int coeff, bmsk=0;
        __m128i zero = _mm_setzero_si128();

        if ((NULL != progvar) && (0==(i&pmsk))) {
	    PROGVARSET(-1.0); /* -1 to indicate beginning of last phase */
	}

        /* find pivot for this row */
        for (aux=v1.data, j=v1.blocks; j; aux++, j--) {
            bmsk = 0xffff ^ _mm_movemask_epi8(_mm_cmpeq_epi8(*aux, zero));
            if (bmsk) break;
        }

        if (0 == bmsk) {
            /* row is zero */
        } else {
            int idx = 0, pos = aux-v1.data, entry;
           
            matrix_collect(&m1, i); /* collect this row */

            // find index of first non-zero entry
            while (0 == (bmsk & 1)) { idx++; bmsk >>= 1; }
            entry = extract_entry(*aux,idx) % prime;
            if (entry<0) entry += prime;
            coeff = pi->inverse[entry]; 
            coeff = prime-coeff;

            /* reduce other vectors in ker */
            v2.data = v1.data + spr; aux = v2.data + pos;
            for (j=i+1; j<ker->rows; j++, v2.data+=spr,  aux+=spr) {
                unsigned val = extract_entry(*aux,idx) % prime; 
                if (val) {
                    int ncoeff = CINTMULT(val,coeff,prime);
                    vector_add(&v2, &v1, ncoeff, prime);
                }
            }
        }
    }

    PROGVARDONE ;

    failure = 0;
 done:; 

    if (failure) return TCL_ERROR;

    if (TCL_OK != matrix_resize(ker, m1.rows)) return TCL_ERROR;

    return SUCCESS;
}
