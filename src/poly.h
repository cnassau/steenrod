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

#include "prime.h"
#include <stdio.h>

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

/* the bitcount macro from the fortune database */
#define BITCOUNT(x)     (((BX_(x)+(BX_(x)>>4)) & 0x0F0F0F0F) % 255)
#define  BX_(x)         ((x) - (((x)>>1)&0x77777777)                    \
                             - (((x)>>2)&0x33333333)                    \
                             - (((x)>>3)&0x11111111))

/* SIGNFUNC computess the sign difference between a*b and a^b */
int SIGNFUNC(int a, int b);

typedef void (*multCBfunc)(void *, mono *m);

void multCBaddToPoly(void *poly, mono *m);

void multPoly(primeInfo *pi, poly *f, poly *s, 
          void *clientData, multCBfunc multCB) ;

#endif
