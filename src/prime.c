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

#include "prime.h"

/* dst = NULL => return number of ints needed, dst != NULL => store data */
int storePrimeInfo(primeInfo *pi, int *dst) {
  int k,l, p=pi->prime, sz;

  sz = 4+3*NALG+p+p*p;
  sz = 8 + (0xffffff8&sz); // fix alignment

  if(NULL == dst) return sz;
  
  dst[0] = storePrimeInfo(pi,NULL);
  dst[1] = pi->prime;
  dst[2] = NALG;
  
  for(k=0;k<NALG;k++) {
    dst[3+0*NALG+k] = pi->primpows[k];
    dst[3+1*NALG+k] = pi->extdegs[k];
    dst[3+2*NALG+k] = pi->reddegs[k];
  }
  for(k=1;k<p;k++)
    dst[3+3*NALG+k] = pi->inverse[k];

  for(k=0;k<p;k++)
    for(l=0;l<p;l++)
      dst[3+3*NALG+p+k*p+l] = pi->binom[k*p+l];
  return 0;
}

/*::: Basic stuff ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

/* primpows, extdegs, reddegs */
int piiBasic(primeInfo *pi) {
    int i, havempx=0;
    xint mpx;
    pi->tpmo = (pi->prime - 1) << 1;
    pi->prime2 = pi->prime * pi->prime;
    if (NULL==(pi->primpows = mallox(sizeof(int) * NALG))) return PI_NOMEM;
    if (NULL==(pi->extdegs  = mallox(sizeof(int) * NALG))) return PI_NOMEM;
    if (NULL==(pi->reddegs  = mallox(sizeof(int) * NALG))) return PI_NOMEM;
    pi->primpows[0] = 1;
    pi->extdegs[0] = 1;
    pi->reddegs[0] = 1;
    for (i=1; i<NALG; i++) {
        pi->primpows[i] = pi->prime * pi->primpows[i-1];
        pi->extdegs[i]  = pi->prime * (pi->extdegs[i-1] + 1) - 1;
        pi->reddegs[i]  = 
            (pi->prime * (pi->reddegs[i-1] * pi->tpmo + 2) - 2) / pi->tpmo;
        mpx = pi->primpows[i];
        if (pi->primpows[i] != mpx) havempx = 1;
        if (!havempx) { pi->maxpowerXint = mpx; pi->maxpowerXintI = i; } 
    }
    return PI_OK;
}

int pidBasic(primeInfo *pi) {
    freex(pi->primpows);
    freex(pi->extdegs);
    freex(pi->reddegs);
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
    if (NULL == (pi->inverse = mallox(sizeof(cint) * pi->prime))) 
        return PI_NOMEM;
    for (i=1; i<pi->prime; i++) 
        if (0 == (pi->inverse[i] = naiveInverse(i, pi->prime)))
            return PI_NOPRIME; /* not prime */
    return PI_OK;
}

int pidInv(primeInfo *pi) {
    freex(pi->inverse);
    return PI_OK;
}

/*::: Binomials ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

int piiBinom(primeInfo *pi) {
    int a,b, prime = pi->prime;
    cint *dat;
    if (NULL==(dat=pi->binom=mallox(sizeof(cint) * pi->prime * pi->prime))) 
        return PI_NOMEM;
 
    pi->binom2 = NULL;
    pi->binom2max = -1;
 
    for (a=prime;a--;) dat[a] = 0;
    dat[0]=1;

    for (b=1;b<prime;b++) {
        for (a=prime-1;a--;) {
            dat[prime*b+a+1] = 
                (dat[prime*(b-1)+a] + dat[prime*(b-1)+a+1]) % prime;
	}
        dat[prime*b] = 1;
    }

    return PI_OK;
}

int pidBinom(primeInfo *pi) {
    freex(pi->binom);
    if (pi->binom2) freex(pi->binom2);
    return PI_OK;
}

/* macro for binomp */  
#define _binomSmall(x, y) (pi->binom[(x)*prime + (y)])

/* computation of binomials; it's important to make sure that this 
 * routine also works for negative inputs */
cint binomp(const primeInfo *pi, int l, int m) {
    unsigned bin = 1, aux;
    cint prime = pi->prime;
    
    while (bin && m) {
        int lquot = l / prime, lrem = l % prime;
        int mquot = m / prime, mrem = m % prime;
#if 0
        ASSERT(l == prime*lquot + lrem);
        ASSERT(m == prime*mquot + mrem);
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

#ifdef USESSE2
cint binompsse(const primeInfo *pi, __m128i l8, __m128i m8) {
    /* TODO: find a smarter implementation */
    xint res = 1;
    union { __m128i li; short l[8]; } ll;
    union { __m128i mi; short m[8]; } mm;
    ll.li = l8; mm.mi = m8;
#define DOIT(k) \
    res *= binomp(pi,ll.l[k],mm.m[k]); res %= pi->prime; if (!res) return 0;  
    DOIT(0);
    DOIT(1);
    DOIT(2);
    DOIT(3);
    DOIT(4);
    DOIT(5);
    DOIT(6);
    DOIT(7);
#undef DOIT
    return res;
}
#endif

#define BINOMP2IDX(n,m) ((((n)+1)*((n)+2))+(m))

int collisionidx(int prime, int n, int m) {
    int idx = 0;
    while (n && m) {
	if( (n % prime) + (m % prime) >= prime ) {
	    return idx;
	}
	n = n/prime;
	m = m/prime;
	idx++;
    }
    return 0;
}

void piiMakeBinom2(primeInfo *pi, int l) {
    cint *newdat, *olddat = pi->binom2;
    int reqsz = (l+2)*(l+1), i,j, p = pi->prime, p2 = pi->prime2;
    if (NULL == olddat) {
	newdat = mallox(sizeof(cint)*2*reqsz);
	*newdat = -1; *(newdat+1) = 1; /* "0 over 0" */
	pi->binom2max = 0;
    } else {
	newdat = reallox(olddat,sizeof(cint)*2*reqsz);
    }
    if (NULL == newdat) return;
    pi->binom2 = newdat;
    for (i=pi->binom2max+1;i<=l;i++) {   
	cint *newrow = newdat+i*(i+1), *lastrow = newdat+(i-1)*i;
	newrow[0] = -1; newrow[1] = 1;
	for (j=1;j<i;j++) {
	    cint aux = newrow[2*j+1] = (lastrow[2*j-1] + lastrow[2*j+1]) % p2;
	    if (aux % p) {
		newrow[2*j] = -1;
	    } else {
		newrow[2*j] = collisionidx(pi->prime, i-j,j);
	    }
	}
	newrow[2*i] = -1;
	newrow[2*i+1] = 1;
    }
    pi->binom2max = l;
}

/* binomials modulo p^2 */
cint binomp2(primeInfo *pi, int l, int m, int *collision) {
    cint *aux;
    if (m>l || m<0 || l<0) {
	*collision = -1;
	return 0;
    }
    if (l>pi->binom2max) {
	piiMakeBinom2(pi,pi->prime*l);
    }
    if (l>pi->binom2max) {
	*collision = -1;
	return 0;
    }
    aux = pi->binom2+l*(l+1)+2*m;
    *collision = *aux;
    return *(aux+1);
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

int makePrimeInfo(primeInfo *pi, int prime) {
    int i, rval; 
    if (prime<2) return PI_NOPRIME;  
    if (prime>61) return PI_TOOLARGE;  
    pi->prime  = prime; 
    pi->maxdeg = 0; /* TODO: compute right vaue */
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
    double rint = rand();
    rint /= RAND_MAX; 
    rint *= prime;
    return (cint) rint;
}

int extdeg(const primeInfo *pi, int msk) {
    int res=0, i=NALG, wrk=1<<i;
    while (i--,wrk>>=1) 
        if (0 != (wrk&msk))
            res += pi->extdegs[i];
    return res;
}

/* SIGNFUNC computes the difference between "a*b" and "a^b" in the 
 * exterior part */
int SIGNFUNC(int a, int b) {
    int res = 0, cnt = 0;
    /* go through bits in a */
    while (a) {
        int z = a ^ (a & (a-1)); /* lowest bit in a */
        cnt++;                   /* count number of bits in a */
        res += BITCOUNT((z-1) & b); /* number of bits in b we have to skip */
        a &= (a-1); b |= z;
    }
    /* implicitly we've had to reverse a, since it's easier to isolate the
     * lowest bit than the highest. Here we make up for the reversion: */
    res += (2 & cnt) ? 1 : 0;
    return res;
}
