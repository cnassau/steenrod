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

#include "prime.h"

/*::: Basic stuff ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

/* primpows, extdegs, reddegs */
int piiBasic(primeInfo *pi) {
    int i, N;
    for (i=1, N=1; i<=pi->maxdeg; i*=pi->prime, N++) pi->N = N;
    pi->tpmo = (pi->prime - 1) << 1;
    if (NULL==(pi->primpows = cmalloc(sizeof(int) * pi->N))) return PI_NOMEM;
    if (NULL==(pi->extdegs  = cmalloc(sizeof(int) * pi->N))) return PI_NOMEM;
    if (NULL==(pi->reddegs  = cmalloc(sizeof(int) * pi->N))) return PI_NOMEM;
    pi->primpows[0] = 1;
    pi->extdegs[0] = 1;
    pi->reddegs[0] = 1;
    for (i=1; i<pi->N; i++) {
    pi->primpows[i] = pi->prime * pi->primpows[i-1];
    pi->extdegs[i]  = pi->prime * (pi->extdegs[i-1] + 1) - 1;
    pi->reddegs[i]  = pi->prime * (pi->reddegs[i-1] + 2) - 2;
    }
    return PI_OK;
}

int pidBasic(primeInfo *pi) {
    cfree(pi->primpows);
    cfree(pi->extdegs);
    cfree(pi->reddegs);
    return PI_OK;
} 

/*::: Inverses :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

cint naiveInverse(int i, cint prime) {
    int j;
    cint aux;
    for (j=1, aux=i; j<prime; j++, aux += i) {
    aux = aux % prime; 
    if (1 == aux) return j;
    } 
    return PI_OK;
}

int piiInv(primeInfo *pi) {
    int i;
    if (NULL == (pi->inverse = cmalloc(sizeof(cint) * pi->prime))) 
    return PI_NOMEM;
    for (i=1; i<pi->prime; i++) 
    if (0 == (pi->inverse[i] = naiveInverse(i, pi->prime)))
        return PI_NOPRIME; /* not prime */
    return PI_OK;
}

int pidInv(primeInfo *pi) {
    cfree(pi->inverse);
    return PI_OK;
}

/*::: Binomials ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

int piiBinom(primeInfo *pi) {
    int a,b, prime = pi->prime;
    cint *dat;
    if (NULL==(dat=pi->binom=cmalloc(sizeof(cint) * pi->prime * pi->prime))) 
    return PI_NOMEM;

    for (a=prime;a--;) dat[a] = 0;
    dat[0]=1;

    for (b=1;b<prime;b++) {
        for (a=prime-1;a--;)
            dat[prime*b+a+1] = dat[prime*(b-1)+a] + dat[prime*(b-1)+a+1] ;
        dat[prime*b] = 1;
    }

    return PI_OK;
}

int pidBinom(primeInfo *pi) {
    cfree(pi->binom);
    return PI_OK;
}

/* macro for binomp */  
#define _binomSmall(x, y) (pi->binom[(x)*prime + (y)])

/* computation of binomials; it's important to make sure that this 
 * routine also works for negative inputs */
cint binomp(primeInfo *pi, int l, int m) {
    unsigned bin = 1, aux;
    cint prime = pi->prime;
    
    while (bin && m) {
        int lquot = l / prime, lrem = l % prime;
        int mquot = m / prime, mrem = m % prime;
#if 0
        assert(l == prime*lquot + lrem);
        assert(m == prime*mquot + mrem);
#endif
        if (lrem<0) { lrem += prime; lquot--; }
        if (mrem<0) { mrem += prime; mquot--; }
        if (-1==m) {
            if (-1!=l) bin = 0;
            break;
        }
        bin *= (aux = _binomSmall(lrem, mrem));
        l -= lrem; l = lquot; m-= mrem; m = mquot;
        bin %= prime;
    }
    return bin;
}

/*::: Control structure ::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

typedef int (*piFunc) (primeInfo *pi);  /* return nonzero on failure */

typedef struct { 
    piFunc ini;   /* constructor */
    piFunc done;  /* destructor */
} piProcEntry;

/* constructor/destructor pairs for the creation of primeInfo structs */
piProcEntry piList[] = {
    { piiBasic, pidBasic },  /* primpows, extdegs, reddegs */
    { piiInv,   pidInv,  },
    { piiBinom, pidBinom } 
};

int makePrimeInfo(primeInfo *pi, int prime, int maxdeg) {
    int i, rval; 
    if (prime<2) return PI_NOPRIME;  
    if (prime>61) return PI_TOOLARGE;  
    pi->prime  = prime; 
    pi->maxdeg = maxdeg;
    for (i=0; i<sizeof(piList)/sizeof(piProcEntry); i++) 
    if (PI_OK != (rval = piList[i].ini(pi))) {
        for (;i--;) piList[i].done(pi);
        return rval;
    }
    return PI_OK;
}

int disposePrimeInfo(primeInfo *pi) {
    int i;
    for (i=sizeof(piList)/sizeof(piProcEntry); i--;) 
    if (piList[i].done(pi)) 
        return PI_STRANGE;
    return PI_OK;
}

/*::: Small stuff ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

cint random_cint(cint prime) {
    double rint = random();
    rint /= RAND_MAX; 
    rint *= prime;
    return (cint) rint;
}

int extdeg(primeInfo *pi, int msk) {
    int res=0, i=pi->N, wrk=1<<i;
    while (i--,wrk>>=1) 
        if (0 != (wrk&msk))
            res += pi->extdegs[i];
    return res;
}

