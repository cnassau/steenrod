/*
 * Monomials, polynomials, and basic operations
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

#ifndef POLY_DEF
#define POLY_DEF

#include "common.h"
#include "prime.h"
#include <stdio.h>

/* An extended monomial represents the tuple
 *
 *     (coefficient, exterior part, exponent sequence, generator id)
 **/

typedef struct {
    int coeff;
    int ext;
    union {
        int dat[NALG];
    } r;
    int gen;
} exmo;

/* For us the "padding value" of an exponent sequence (r1, r2, r3,...) is
 * the common value of the rj for j large. Its length is the biggest 
 * j for which "rj != padding value". */

int exmoGetLen(exmo *e);    /* look at reduced and exterior parts */
int exmoGetRedLen(exmo *e); /* look only at reduced part */
int exmoGetPad(exmo *e);

/* the following functions check if ">=" (resp. "<=") holds for all exponents */
int exmoIsAbove(const exmo *a, const exmo *b);
int exmoIsBelow(const exmo *a, const exmo *b);

int compareExmo(const void *aa, const void *bb);

void copyExmo(exmo *dest, const exmo *src);
void clearExmo(exmo *e);

#define USENEGSHIFT 2 /* reflect arguments at -1 */
#define ADJUSTSIGNS 1 /* should we just xor the exterior data,
                       * or should we simulate an exterior algebra? */
void shiftExmo(exmo *e, const exmo *shft, int flags);
void shiftExmo2(exmo *e, const exmo *shft, int scale, int flags);

/* compute internal degree with respect to pi */
int exmoIdeg(primeInfo *pi, const exmo *ex);
int exmoRdeg(primeInfo *pi, const exmo *ex);

/* set dst to the maximal/minimal algebra profile */
void exmoSetMaxAlg(primeInfo *pi, exmo *dst);
void exmoSetMinAlg(primeInfo *pi, exmo *dst);

/* make "exponential copy" */
void copyExpExmo(primeInfo *pi, exmo *dst, const exmo *src);

/* pprop stands for "polynom property"; one can check for these 
 * properties using the "test" member function of the polyType 
 * structure. The test function returns SUCCESS (resp. FAILUNTRUE)
 * if the property is known to be true (resp. false). Other return
 * codes indicate that the test is not possible. */

typedef enum {
    ISPOSITIVE,  /* all exponents >= 0, exterior component >= 0 */
    ISNEGATIVE,  /* all exponents  < 0, exterior component  < 0 */
    ISPOSNEG     /* ISPOSITIVE or ISNEGATIVE */
} pprop;

typedef struct {
    const char *name;
    size_t bytesAllocated;
    size_t bytesUsed; 
    /* the fields below need not be initialized 
     * by the getInfo member function */
    int maxRedLength;
} polyInfo;

/* A polynomial is an arbitrary collection of extended monomials. 
 * Different realizations are thinkable (compressed vs. uncompressed, 
 * vector vs. list style, etc...), so we try to support many different 
 * implementations; such an implementation type is described opaquely 
 * by a polyType structure. */

typedef struct polyType {

    int (*getInfo)(void *self, polyInfo *poli); /* obvious, eh? */

    int (*getMaxRedLength)(void *self, int *len); /* max red-length of all exmo's */

    void *(*createCopy)(void *src);  /* create a copy; if src is NULL a
                                      * new empty polynomial is created */

    void (*swallow)(void *self, 
                    void *victim);   /* make self a copy of victim
                                      * which can be cannibalized  */  

    void (*clear)(void *self);       /* set self to zero */

    void (*free)(void *self);        /* free all allocated storage */

    int (*getNumsum)(void *self);    /* return number of summands */

    int (*getExmo)(void *self, 
                   exmo *exmo, 
                   int index);       /* retrieve extended monomial 
                                      * from given index */

    int (*getExmoPtr)(void *self, 
                      exmo **exmo, 
                      int index);    /* try to get in-place pointer to 
                                      * a summand (read-only pointer!) */

    int (*collectCoeffs)(void *self,
                         const exmo *e,
                         int *coeff,
                         int mod, 
                         int flags); /* determine coefficient of e modulo mod */

    int (*lookup)(void *self, 
                  const exmo *e,
                  int *coeff);       /* search for *e in *self and return its 
                                      * index number. return -1 if *e is not 
                                      * contained in *self. store coefficient 
                                      * of *e in *coeff if coeff is not zero.
                                      * this function usually requires a sorted 
                                      * polynomial, so cancel should have been
                                      * previously invoked. */

    int (*remove)(void *self, int idx); /* remove summand number idx */

    int (*test)(void *self, pprop p);  /* test for property */ 

    void (*cancel)(void *self, int modulo);  /* cancel as much as possible */

    void (*reflect)(void *self);  /* let (new exp.) = -1 - (old exp.) */

    int  (*compare)(void *pol1, 
                    void *pol2,
                    int *result,
                    int flags);    /* comparison result as in qsort */

    int  (*appendPoly)(void *self, 
                       void *other, 
                       const exmo *shift,
                       int shiftflags,
                       int scale, 
                       int modulo);    /* append scaled version of another 
                                        * poly of the same type, possibly
                                        * shifted by a given exmo */

    int  (*appendExmo)(void *self, 
                       const exmo *exmo); /* append an extended monomial */

    int  (*scaleMod)(void *self, 
                     int scale, 
                     int modulo);  /* scale poly and reduce (if modulo != 0) */

    void (*shift)(void *self, 
                  const exmo *shift,
                  int shiftflags); /* shift entire polynomial */

} polyType;

/* Some flags values */
#define PLF_ALLOWMODIFY 1  /* modification allowed */

/* wrappers for the polyType member functions; these check whether 
 * the member function is non-zero, and try to use work-arounds if 
 * a method is not implemented */

int   PLgetInfo(polyType *type, void *poly, polyInfo *poli);
int   PLgetNumsum(polyType *type, void *poly);
int   PLgetMaxRedLength(polyType *type, void *poly);
void  PLfree(polyType *type, void *poly);
int   PLcancel(polyType *type, void *poly, int modulo);
int   PLclear(polyType *type, void *poly);
void *PLcreate(polyType *type);
int   PLtest(polyType *tp1, void *pol1, pprop prop);
int   PLcompare(polyType *tp1, void *pol1, polyType *tp2, 
                void *pol2, int *result, int flags);
int   PLgetExmo(polyType *type, void *self, exmo *exmo, int index);
int   PLcollectCoeffs(polyType *type, void *self, const exmo *exmo, 
                      int *rval, int mod, int flags);
int   PLappendExmo(polyType *dtp, void *dst, const exmo *e);
int   PLappendPoly(polyType *dtp, void *dst, 
                   polyType *stp, void *src,                      
                   const exmo *shift, int shiftflags,
                   int scale, int modulo);
void *PLcreateCopy(polyType *newtype, polyType *oldtype, void *poly);

/* The multiplication routines take as input descriptions (fftp,ff) resp.
 * (sftp,sf) of the first factor (ff) resp. second factor (sf). If successful
 * they create a new polynomial and store its type (resp. data pointer)
 * in *rtp (resp. *res). */
int   PLposMultiply(polyType **rtp, void **res,
                    polyType *fftp, void *ff,
                    polyType *sftp, void *sf, int mod);
int   PLnegMultiply(polyType **rtp, void **res,
                    polyType *fftp, void *ff,
                    polyType *sftp, void *sf, int mod);
int   PLsteenrodMultiply(polyType **rtp, void **res,
                         polyType *fftp, void *ff,
                         polyType *sftp, void *sf, 
                         primeInfo *pi, const exmo *pro);

#ifndef POLYC 
extern polyType stdPolyType;
#endif

#define stdpoly (&(stdPolyType))

/* create a stdpoly copy of a polynomial */
void *PLcreateStdCopy(polyType *type, void *poly);

/* we use stdpoly to implement monomial maps; here is a sort function that 
 * simultaneously rearranges an extra table of associated values */
int stdpolySortWithValues(void *poly, void **values);

#endif
