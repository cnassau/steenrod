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

#ifndef LINALG_DEF
#define LINALG_DEF

#include "prime.h"

/* we use two types: vectors and matrices */

typedef struct {
    int num;
    cint *data;
} vector;

/* nomcols stands for "nominal columns"; allows to allocate space 
 * for more columns than actually present (whatever this might be 
 * useful for ...?) */
typedef struct {
    int rows, cols; 
    int nomcols;    
    cint *data;
} matrix;

vector * vector_create( int size );
void vector_dispose( vector *v );
void vector_clear( vector *v );
int  vector_iszero( vector *v );
void vector_copy( vector *v, vector *w );
void vector_add( vector *dst, vector *src, cint coeff, cint prime );
void vector_add_entry( vector *dst, int off, cint dat, cint prime );
cint vector_get_entry( vector *src, int off );
void vector_randomize( vector *v, cint prime );

matrix *matrix_create( int rows, int cols );
void matrix_destroy( matrix *mat );
void matrix_clear( matrix *mat );    /* clear matrix */
void matrix_unit( matrix *mat );     /* make unit matrix */
int  matrix_resize( matrix *m, int newrows ); /* reallocate space */
void matrix_randomize( matrix *m, cint prime );

/* matrix_collect is used whenever we want to throw away some
 * of the rows; for this we first set m->rows = 0, then call 
 * matrix_collect(...) for those rows that we want to keep.    */
void matrix_collect( matrix *m, int r );

#endif
