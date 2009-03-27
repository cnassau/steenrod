/*
 * Steenrod algebra multiplication routine
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

#ifndef MULT_DEF
#define MULT_DEF

#include "common.h"
#include "prime.h"
#include <stdio.h>

#include "poly.h"

/* If you read this you'd better know how Milnor's multiplication algorithm  
 * works. It uses the "multiplication matrices" X = (x_ij) that fit a given 
 * pair of factors (A,B). The multiplicator iterates through these matrices 
 * and some of them lead to summands of the product A * B. Check out the
 * canonical sources for more details. */

/* A Xfield represents a xi-box in the multiplication matrix. It
 *
 *  1)  holds a value (val)
 *  2)  knows where the mask of "forbidden bits" is stored (*oldmsk, *newmsk)
 *  3)  knows where the sum is kept (*sum, sum_weight)
 *  4)  knows where the reservoir for this row or column is (*res, res_weight)
 *
 * added extra features:
 *
 *  5)  "quantization of values", used to preserve profiles (quant)
 */

typedef struct {
    xint val,
        quant,
        *oldmsk, *newmsk,
        *sum,
        *res;
    int sum_weight, res_weight, ext_weight;
    int estat;
} Xfield;

/* At odd primes the algorithm needs an additional matrix E=(e_ij) of 1-bit
 * integers. However, these are stored in an ad-hoc manner so there's no 
 * dedicated Efield structure. */

/* Our multiplication routine is based on the extensive use of callback functions.
 * To multiply two polynomials (i.e. Steenrod algebra elements) A and B one has 
 * to provide two (usually three) such callback functions:
 *
 * (assuming that A is positive)
 *
 *   func1: this iterates through the summands of A
 *
 * For each such summand the program then determines all allowable multiplication
 * matrices X (see Margolis' book for a descripton of these.)  
 *
 *   func2: this one is called for each such multiplication matrix.
 *            
 * There is a standard func2 implementation that delegates its work to
 *   
 *       func2.1: this iterates through the summands of B and checks whether it 
 *                has found a summand of the product.
 *     
 *       func2.2: this is called for each such summand. 
 *
 * There is a standard func2.2 implementation that just adds the summand to 
 * a given polynomial.
 *
 * If A is negative (ie. all its exponents are negative, meaning that A is actually
 * an element of the dual Steenrod algebra and we're acting on it from the right
 * by B), a similar cascade of callbacks is used; however, in that case the 
 * multiplication matrices are derived from B and the summand fetching callbacks 
 * iterate through the summands of A.
 *
 * The input to the multiplication engine is given by the following multArgs 
 * structure: */

typedef struct multArgs {
    /* pi and profile have to remain valid during multiplication */
    primeInfo *pi;       /* describes the prime */
    exmo      *profile;  /* the subalgebra profile that we want to respect */
    int        prime;    /* same as pi->prime, provided for faster access */
    
    int        collision; /* collision index, used for EBP */
    int        collisionAllowed; 

    /* first factor data & callbacks */
    void *ffdat, *ffdat2;
    int   ffIsPos, ffMaxLength;
    int (*getExmoFF)(struct multArgs *self, int factor, const exmo **ex, int idx);
    void (*fetchFuncFF)(struct multArgs *self, int coeff); 
    
    /* second factor data & callbacks */
    void *sfdat, *sfdat2;
    int   sfIsPos, sfMaxLength; 
    int (*getExmoSF)(struct multArgs *self, int factor, const exmo **ex, int idx);
    void (*fetchFuncSF)(struct multArgs *self, int coeff); 

    /* ids of both factors. These are adjusted by the workXYchain functions. */
    int ffid, sfid;

    /* The multiplication matrix is stored here. For historical reasons
     * there are actually two matrices, one for "AP" and one for "PA". 
     * Here AP and PA stands for "any times positive" resp. "positive times any". */

    Xfield xfPA[NALG+1][NALG+1], 
        xfAP[NALG+1][NALG+1];

    xint msk[NALG+1][NALG+1], 
        sum[NALG+1][NALG+1];

    int emsk[NALG+1], 
        esum[NALG+1];

    /* client data -- interpretation is up to the callbacks */
    void *cd1, *cd2, *cd3, *cd4, *cd5;
    exmo fcdexmo, scdexmo;

    /* reserved for a "Tcl_Interp *" that could be used for error reporting */
    void *TclInterp; 

    /* callbacks that add the summands to a poly can use these: */
    void     *resPolyPtr; 
    polyType *resPolyType;

    /* the function that's invoked by the stundard fetch funcs */
    void (*stdSummandFunc)(struct multArgs *self, const exmo *smd);
} multArgs;

/* The standard getExmo function is just a wrapper for PLgetExmo.
 * Depending on "factor", it interprets the pair (ffdat, ffdat2) 
 * [resp. (sfdat, sfdat2)] as (polyType, poly) */
#define FIRST_FACTOR  1
#define SECOND_FACTOR 2
int stdGetExmoFunc(struct multArgs *self, int factor, const exmo **ex, int idx);

/* This callback is used to implement a single exmo. Its address should 
 * be kept in the ffdat field (resp sfdat for the second factor). */
int stdGetSingleExmoFunc(multArgs *ma, int factor, const exmo **ret, int idx);

/* Here are the standard fetch funcs */
void stdFetchFuncFF(struct multArgs *self, int coeff); 
void stdFetchFuncSF(struct multArgs *self, int coeff); 

/* An implementation of a stdSummandFunc that adds the summand to a polynomial */
void stdAddSummandToPoly(struct multArgs *self, const exmo *smd);

/* The next function sets up the multiplication matrices: */
void initMultargs(multArgs *ma, primeInfo *pi, exmo *profile);

/* These functions start the computation: */
void workPAchain(multArgs *ma);
void workAPchain(multArgs *ma);

/* finally, one invocaton that puts it all together */
int stdAddProductToPoly(polyType *rtp, void *res,
                        polyType *ftp, void *ff,
                        polyType *stp, void *sf,
                        primeInfo *pi, const exmo *pro,
                        int fIsPos, int sIsPos);

int stdAddProductToPolyEBP(polyType *rtp, void *res,
			   polyType *ftp, void *ff,
			   polyType *stp, void *sf,
			   primeInfo *pi);

/* the following counter tries to estimate the number
 * of multiplications that have been carried out */
#ifndef MULTC_INCLUDES
extern
#endif
int multCount;

#endif
