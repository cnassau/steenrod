/*
 * Advanced linear algebra 
 *
 * Copyright (C) 2004-2006 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef ADLINALG_DEF
#define ADLINALG_DEF

#include <tcl.h>
#include "linalg.h"

/* The routines below often need a long time to complete; if progvar is 
 * nonzero they will regularly put progress information into the variable 
 * *progvar. This allows to monitor their progress from Tcl */

/* orthonormalize the input matrix, return basis of kernel 
 * and (optionally) the base change description */
matrix *matrix_ortho(primeInfo *pi, matrix *inp, matrix **urb, 
                     Tcl_Interp *ip, const char *progvar, int pmsk);

/* orthonormalize inp and reduce rows of lft against it; 
 * returns preimages for these rows if they reduce to zero. */
matrix *matrix_lift(primeInfo *pi, matrix *inp, matrix *lft, 
             Tcl_Interp *ip, const char *progvar, int pmsk);

/* reduce ker modulo im; im is assumed to be the result of matrix_ortho,
 * so we can take the obvious pivots for it; the result is returned in ker,
 * which is shrunken automatically. */
int matrix_quotient(primeInfo *pi, matrix *ker, matrix *im, 
             Tcl_Interp *ip, const char *progvar, int pmsk);

#endif
