/*
 * Basic information that depends on the prime
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

#ifndef PRIME_DEF
#define PRIME_DEF

#include <stdlib.h>
#define cmalloc malloc
#define cfree free

/* cint ist the integer type that we use for elements of F_p */
typedef char cint; 

#define CINTMULT(a,b,prime) ((cint) (( (short (a)) * (short (b)) ) % ((short) prime)))

typedef struct {
    cint prime;
    int maxdeg;      /* maximum dimension for which this data is complete */
    /* basic */
    int N;           /* minimum length of the arrays below: */
    int *primpows;   /* prime powers */
    int *extdegs;    /* exterior degrees */
    int *reddegs;    /* degrees of the reduced part, divided by 2(p-1) */
    int tpmo;        /* 2(p-1) */
    /* inverse */
    cint *inverse;   /* table of inverses */
    /* binom */
    cint *binom;     /* table of binomials (a over b) with 0 <= a,b < prime */
    /* seqno */
    int rseqnum;     /* number of necessary tables */
    int **rseqtab;   /* helper tables for the seqno function */
} primeInfo;

/* possible return values of makePrimeInfo: */

#define PI_OK         0
#define PI_NOPRIME    1
#define PI_TOOLARGE   2
#define PI_NOMEM      3
#define PI_STRANGE    4

int makePrimeInfo( primeInfo *pi, int prime, int maxdeg );
int disposePrimeInfo( primeInfo *pi );

/* compute binomial "l over m" mod pi->prime */
cint binomp( primeInfo *pi, int l, int m );

#endif
