/*
 * Monomials, polynomials, and basic operations
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
    int dat[NALG];
    int gen;
} exmo;

/* For us the "padding value" of an exponent sequence (r1, r2, r3,...) is
 * the common value of the rj for j large. Its length is the biggest 
 * j for which "rj != padding value". */

int exmoGetLen(exmo *e);
int exmoGetPad(exmo *e);

void copyExmo(exmo *dest, exmo *src);

#define ADJUSTSIGNS 1 /* should we just xor the exterior data,
                       * or should we simulate an exterior algebra? */
void shiftExmo(exmo *e, const exmo *shft, int flags);

/* A polynomial is an arbitrary collection of extended monomials. 
 * Different realizations are thinkable (compressed vs. uncompressed, 
 * vector vs. list style, etc...), so we try to support many different 
 * implementations; such an implementation type is described opaquely 
 * by a polyType structure. */

typedef struct polyType {
    void *(*createCopy)(void *src);  /* create a copy; if src is NULL a
                                      * new empty polynomial is created */

    void (*swallow)(void *self, 
                    void *victim);   /* make self a copy of victim
                                      * which can be cannibalized  */  

    void (*clear)(void *self);       /* set self to zero */

    void (*free)(void *self);        /* free all allocated storage */

    int (*getLength)(void *self);    /* return number of summands */

    int (*getExmo)(void *self, 
                   exmo *exmo, 
                   int index);       /* retrieve extended monomial 
                                      * from given index */

    int (*getExmoPtr)(void *self, 
                      exmo **exmo, 
                      int index);    /* try to get in-place pointer to 
                                      * a summand (read-only pointer!) */

    void (*cancel)(void *self, int modulo);  /* cancel as much as possible */

    int  (*compare)(void *pol1, 
                    void *pol2,
                    int *result);    /* comparison result as in qsort */

    int  (*appendPoly)(void *self, 
                       void *other, 
                       const exmo *shift,
                       int shiftflags,
                       int scale, 
                       int modulo);    /* append scaled version of another 
                                        * poly of the same type, possibly
                                        * shifted by a given exmo */

    int  (*appendExmo)(void *self, exmo *exmo); /* append an extended monomial */

    int  (*scaleMod)(void *self, 
                     int scale, 
                     int modulo);  /* scale poly and reduce (if modulo != 0) */
} polyType;

/* wrappers for the polyType member functions; these check whether 
 * the member function is non-zero, and try to use work-arounds if 
 * a method is not implemented */

int   PLgetLength(polyType *type, void *poly);
void  PLfree(polyType *type, void *poly);
int   PLcancel(polyType *type, void *poly, int modulo);
int   PLclear(polyType *type, void *poly);
void *PLcreate(polyType *type);
int   PLcompare(polyType *tp1, void *pol1, polyType *tp2, void *pol2, int *result);
int   PLgetExmo(polyType *type, void *self, exmo *exmo, int index);
int   PLappendExmo(polyType *dtp, void *dst, exmo *e);
int   PLappendPoly(polyType *dtp, void *dst, 
                   polyType *stp, void *src,                      
                   const exmo *shift, int shiftflags,
                   int scale, int modulo);
void *PLcreateCopy(polyType *newtype, polyType *oldtype, void *poly);

#ifndef POLYC
extern polyType stdPolyType;
#endif

#define stdpoly (&(stdPolyType))

/* create a stdpoly copy of a polynomial */
void *PLcreateStdCopy(polyType *type, void *poly);

/* old stuff follows */

/* monomial = coefficient + exterior part + exponent sequence + generator id */
typedef struct {
    xint coeff;
    xint ext;
    xint dat[NALG];
    xint id;
} mono;

/* polynomial = array of mono's */
typedef struct {
    xint maxcoeff;  /* coefficients are reduced modulo maxcoeff */
    int num,        /* number of summands */
    numAlloc;   /* number of allocated mono's */
    mono *dat;
} poly;

void polyRaisePPow(poly *p, int exp);
void polyShiftEntry(poly *p, int idx, xint val);
poly *createPoly(int alloc) ;
void clearPoly(poly *p) ;
void clearMono(mono *p) ;
int reallocPoly(poly *p, int alloc) ;
void disposePoly(poly *p) ;
void copyMono(mono *a, mono *b) ;
int appendMono(poly *p, mono *m) ;
int appendPoly(poly *p, poly *m) ;
int appendScaledPoly(poly *p, poly *m, xint scaleFactor) ;
int compareMono(const void *bb, const void *aa) ;
int compareMonoWithAddress(const void *bb, const void *aa) ;
void sortPoly(poly *p) ;
void compactPoly(poly *p) ;
void multMonoPosPoly(mono *r, mono *a, mono *b) ;
void multMonoNegPoly(mono *r, mono *a, mono *b) ;
typedef void (*monoCompTwoFunc)(mono *, mono *, mono *);
typedef void (*monoCompOneFunc)(mono *, mono *);
int polyAppendMult(poly *r, poly *a, poly *b, monoCompTwoFunc fnc) ;
int polyCompCopy(poly *tar, poly *src, monoCompOneFunc fnc) ;
void monoReflect(mono *tar, mono *src) ;

typedef void (*multCBfunc)(void *, mono *m);

void multCBaddToPoly(void *poly, mono *m);

void multPoly(primeInfo *pi, poly *f, poly *s, 
          void *clientData, multCBfunc multCB) ;

#endif
