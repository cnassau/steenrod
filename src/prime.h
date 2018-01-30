/*
 * Basic information that depends on the prime
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

#ifndef PRIME_DEF
#define PRIME_DEF

#include "common.h"

#ifdef USESSE2
#  define NALG 8
#else
#  define NALG 8
#endif

#include <stdlib.h>
#define cmalloc  malloc
#define cfree    free
#define crealloc realloc
#define ccalloc  calloc

typedef char cint;   /* data type for elements of F_p  */
typedef short xint;  /* data type for exponents        */

#define CINTMULT(a,b,prime) \
   ((prime) ? ((cint) ((((short) (a)) * ((short) (b))) % ((short) prime))) \
            : ((cint) ((((short) (a)) * ((short) (b))))))

typedef struct {
    cint prime;
    int maxdeg;        /* maximum dimension for which this data is complete */
    /* basic */
    unsigned int *primpows;     /* prime powers */
    unsigned int *extdegs;      /* exterior degrees */
    unsigned int *reddegs;      /* degrees of the reduced part, divided by 2(p-1) */
    unsigned int tpmo;          /* 2(p-1) */
    int maxpowerXint;  /* largest prime power that fits in a xint */
    int maxpowerXintI; /* the I such that primpows[I] == maxpowerXint */
    /* inverse */
    cint *inverse;     /* table of inverses */
    /* binom */
    cint *binom;       /* table of binomials (a over b) with 0 <= a,b < prime modulo prime */
    cint prime2;       /* prime * prime */
    cint *binom2;      /* table of binom(n,m) mod p^2 + collision indices */
    int binom2max;     /* biggest n such that binom(n,*) is available */
 } primeInfo;

/* possible return values of makePrimeInfo: */

#define PI_OK         0
#define PI_NOPRIME    1
#define PI_TOOLARGE   2
#define PI_NOMEM      3
#define PI_STRANGE    4

int makePrimeInfo(primeInfo *pi, int prime);
int disposePrimeInfo(primeInfo *pi);

cint random_cint(cint max);

/* compute binomial "l over m" mod pi->prime */
cint binomp(const primeInfo *pi, int l, int m);

/* compute binomial "l over m" mod (pi->prime)*(pi->prime)
 * also return collision index if binom is zero mod p */
cint binomp2(primeInfo *pi, int l, int m, int *collision);

#ifdef USESSE2
cint binompsse(const primeInfo *pi, __m128i l8, __m128i m8);
#endif

/* return degree of msk with respect to pi->extdegs */
int extdeg(const primeInfo *pi, int msk);

/* the bitcount macro from the fortune database */
#define BITCOUNT(x)     (((BX_(x)+(BX_(x)>>4)) & 0x0F0F0F0F) % 255)
#define  BX_(x)         ((x) - (((x)>>1)&0x77777777)                    \
                             - (((x)>>2)&0x33333333)                    \
                             - (((x)>>3)&0x11111111))

/* SIGNFUNC computes the sign difference between a*b and a^b 
 * in an exterior algebra */
int SIGNFUNC(unsigned int a, unsigned int b);

#endif
