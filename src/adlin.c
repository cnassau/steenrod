/*
 * Advanced linear algebra 
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

#include "adlin.h"

/* common macros for dealing with the progress variable: */
#define PROGVARINIT( val )             \
    double perc;                       \
    Tcl_Obj *ProgVar, *NameObj;        \
    ProgVar = Tcl_NewDoubleObj( val ); \
    NameObj = Tcl_NewStringObj( LAPROGRESSVAR, sizeof(LAPROGRESSVAR) ); \
    Tcl_IncrRefCount( ProgVar );       \
    Tcl_IncrRefCount( NameObj )      

#define PROGVARSET( val ) 	                                      \
  Tcl_SetDoubleObj( ProgVar, perc );                                  \
  if (NULL==Tcl_ObjSetVar2( ip, NameObj, NULL, ProgVar,               \
			    TCL_LEAVE_ERR_MSG | TCL_GLOBAL_ONLY ))    \
       goto done

#define PROGVARDONE \
    Tcl_DecrRefCount( ProgVar ); Tcl_DecrRefCount( NameObj ) 

/* orthonormalize the input matrix, return basis of kernel */
matrix *matrix_ortho( primeInfo *pi, matrix *inp, Tcl_Interp *ip, int pmsk) {
    int i,j,cols, spr, uspr;
    int failure = 1;     /* pessimistic, eh? */
    vector v1,v2,v3,v4;
    cint *aux;
    cint prime = pi->prime;

    matrix m1, m2;
    matrix *un ;

    PROGVARINIT( 0 );

    un = matrix_create( inp->rows, inp->rows );
    if (NULL == un) return NULL;
    matrix_unit( un );

    cols = inp->cols;
    v1.num = v2.num = cols; v3.num = v4.num = un->cols;

    spr = inp->nomcols; /* cints per row */
    uspr = un->nomcols;

    /* make m1, m2 empty copies of inp, resp. un */

    m1.nomcols = inp->nomcols; m1.cols = inp->cols; m1.data = inp->data; m1.rows
 = 0;
    m2.nomcols = un->nomcols;  m2.cols = un->cols;  m2.data = un->data;  m2.rows
 = 0;

    for ( v1.data=inp->data, i=0; i<inp->rows; i++, v1.data+=spr ) {
        cint coeff;
        if ((pmsk) && (0==(i&pmsk))) {
            perc = i; perc /= inp->rows;
            perc = 1-perc; perc *= perc; perc = 1-perc;
	    PROGVARSET( perc );
        }
         /* find pivot for this row */
        for ( aux=v1.data, j=cols; j; aux++, j-- )
            if ( 0 != *aux ) break;
        if ( 0 == j ) {
            /* row is zero */
            matrix_collect( &m2, i ); /* collect kernel vector */
        } else {
            matrix_collect( &m1, i ); /* collect image vector */
            coeff = pi->inverse[(unsigned) *aux]; 
	    coeff = prime-coeff; coeff %= prime;
            /* go through all other rows and normalize */
            v2.data = v1.data + spr; aux += spr;
            v3.data = un->data + i * uspr;
            v4.data = v3.data + uspr;
            for ( j=i+1; j<inp->rows; j++, v2.data+=spr, v4.data+=uspr, aux+=spr
 )
                if ( 0 != *aux ) {
                    vector_add( &v4, &v3, CINTMULT(*aux,coeff,prime), prime );
                    vector_add( &v2, &v1, CINTMULT(*aux,coeff,prime), prime );
                }
        }
    }

    failure = 0; 

 done: 
    if (failure) {
	matrix_destroy( un );
	un = NULL;
    } else {
	if (TCL_OK != matrix_resize( inp, m1.rows )) return NULL;
	if (TCL_OK != matrix_resize( un, m2.rows )) return NULL;
    }

    PROGVARDONE ; 

    return un;
}

