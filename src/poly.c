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

#include <stdlib.h>
#include <string.h>
#include "poly.h"

void clearPoly(poly *p) {
    p->num = 0;
}

int reallocPoly(poly *p, int alloc) {
    mono *ndat;
    if (alloc < p->num) return 0; 
    ndat = crealloc(p->dat, alloc * sizeof(mono));
    if (NULL == ndat) return 0; 
    p->dat = ndat; p->numAlloc = alloc;
    return 1;
}

poly *createPoly(int alloc) {
    return ccalloc(1, sizeof(poly));
}

void disposePoly(poly *p) {
    if (p->numAlloc) cfree(p->dat);
    cfree(p);
}

void clearMono(mono *a) {
    memset(a, 0, sizeof(mono)); 
}

void copyMono(mono *a, mono *b) {
    memcpy(a, b, sizeof(mono)); 
}

int appendMono(poly *p, mono *m) {
    if (p->num >= p->numAlloc) 
        if (!reallocPoly(p, p->numAlloc + 10)) 
            return 0;
    copyMono(p->dat + p->num, m); p->num++;
    return 1;
}

void polyShiftEntry(poly *p, int idx, xint val) {
    int k;
    for (k=0;k<p->num;k++) p->dat[k].dat[idx] += val;
}

int appendPoly(poly *p, poly *m) {
    return appendScaledPoly(p, m, 1);
}
 
int appendScaledPoly(poly *p, poly *m, xint scaleFactor) {
    int k;
    if (0 == scaleFactor) return 1;
    if (p->num + m->num >= p->numAlloc) 
        if (!reallocPoly(p, p->num + m->num + 5)) 
            return 0;
    memcpy(p->dat + p->num, m->dat, m->num * sizeof(mono));
    if (1 != scaleFactor) 
        for (k=m->num;k--;) {
            p->dat[p->num+k].coeff *= scaleFactor;
            if (p->maxcoeff) p->dat[p->num+k].coeff %= p->maxcoeff;
        }
    p->num += m->num;
    return 1;
}

#define COMPRET(x,y) if (0 != (res = (x)-(y))) return res;

int compareMono(const void *bb, const void *aa) {
    mono *a = (mono *) aa;
    mono *b = (mono *) bb;
    int res, n;
    COMPRET(a->id, b->id);
    COMPRET(a->ext, b->ext);
    for (n=NALG; n-- ;) COMPRET(a->dat[n], b->dat[n]);
    return 0;
}

int compareMonoWithAddress(const void *bb, const void *aa) {
    int res = compareMono(aa, bb);
    if (0 != res) return res;
    return (char *) aa - (char *) bb;
}

void sortPoly(poly *p) {
    qsort(p->dat, p->num, sizeof(mono), compareMonoWithAddress);
}

void monoRaisePPow(mono *m, int exp) {
    int k;
    for (k=NALG;k--;) m->dat[k] = -1 - exp * (-1 - m->dat[k]); 
}

void polyRaisePPow(poly *p, int exp) {
    int k;
    for (k=p->num;k--;) monoRaisePPow(&(p->dat[k]), exp);
}

void compactPoly(poly *p) {
    mono *src, *tar, *nxt, *top; 
    xint cf, cfmask; 
    sortPoly(p);
    src = p->dat; top = p->dat + p->num;
    tar = p->dat;
    cfmask = p->maxcoeff;
    while (src < top) {
        cf = src->coeff;
        if (cfmask) { 
            cf %= cfmask;
            if (cf<0) cf += cfmask;
        }
        /* collect all coefficients for this monomial */
        for (nxt = src+1; nxt<top; nxt++) {
            if (0 != compareMono(src, nxt)) break; 
            cf += nxt->coeff; 
            if (cfmask) cf %= cfmask;
        }
        if (0 != cf) { copyMono(tar, src); tar->coeff = cf; tar++; }
        src = nxt; 
    }
    p->num = tar - p->dat ;
}

void multMonoPosPoly(mono *r, mono *a, mono *b) {
    int i;
    r->id = a->id;
    r->coeff = a->coeff * b->coeff ;
    if (0 != (a->ext & b->ext)) r->coeff = 0;
    r->ext = a->ext | b->ext;
    if (0 != (1 & SIGNFUNC(a->ext, b->ext))) r->coeff = - r->coeff ;
    for (i=NALG;i--;) r->dat[i] = a->dat[i] + b->dat[i];
}

void multMonoNegPoly(mono *r, mono *a, mono *b) {
    int i;
    xint ae = ~a->ext, be = ~b->ext;
    r->id = a->id;
    r->coeff = a->coeff * b->coeff ;
    /* if (0 != (ae & be)) r->coeff = 0; */
    r->ext = ae | be;
    if (0 != (1 & SIGNFUNC(ae, be))) r->coeff = - r->coeff ;
    r->ext = ~r->ext;
    for (i=NALG;i--;) r->dat[i] = a->dat[i] + b->dat[i] + 1;
}

int polyAppendMult(poly *r, poly *a, poly *b, monoCompTwoFunc fnc) {
    int i,j;
    mono aux;
    for (i=0; i<a->num; i++)
        for (j=0; j<b->num; j++) {
            fnc(&aux, a->dat + i, b->dat + j);
            if (r->num >= r->numAlloc) compactPoly(r);
            if (!appendMono(r, &aux)) return 0;
        }
    return 1;
}

int polyCompCopy(poly *tar, poly *src, monoCompOneFunc fnc) {
    int i;
    if (tar->numAlloc < src->num)
        if (! reallocPoly(tar, src->num)) 
            return 0;
    for (i=0; i<src->num; i++) 
        fnc(tar->dat + i, src->dat + i);
    tar->num = src->num;
    return 1;
}

void monoReflect(mono *tar, mono *src) {
    int i;
    tar->coeff = src->coeff; tar->ext = src->ext; tar->id = src->id; 
    for (i=NALG; i--;) tar->dat[i] = -1 - src->dat[i];
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

/* callback that adds mono to poly */
void multCBaddToPoly(void *pol, mono *m) {
    poly *p = (poly *) pol; 
    appendMono(p, m);
}

typedef struct {
    int fIsPos, sIsPos; /* flags */
    primeInfo *pi; 
    mono *profile;      /* the profile that we want to respect */
    poly *f, *s;        /* first and second factor */
    void *clientData;   /* passed to multCB */
    multCBfunc multCB;  /* callback */
} multArgs ;

void multAnyPos(multArgs *MA, mono *s) ;
void multPosAny(multArgs *MA, mono *f) ;

void multPoly(primeInfo *pi, poly *f, poly *s, 
              void *clientData, multCBfunc multCB) {
    int k; multArgs MA; xint msk[NALG+1], sum[NALG+1];
    MA.clientData = clientData;
    MA.multCB = multCB;
    MA.f = f; MA.s = s; 
    MA.pi = pi;
    if (0 == f->num) return;
    if (0 == s->num) return;
    MA.fIsPos = (f->dat[0].dat[0] >= 0);
    MA.sIsPos = (s->dat[0].dat[0] >= 0);
    MA.profile = NULL; 
    for (k=NALG+1;k--;) msk[k] = sum[k] = 0;
    if (!MA.fIsPos) {
        for (k=0;k<s->num;k++) 
            multAnyPos(&MA, &(s->dat[k]));
    } else {
        for (k=0;k<f->num;k++) 
            multPosAny(&MA, &(f->dat[k]));
    }
}

inline xint XINTMULT(xint a, xint b, xint prime) { 
    int aa = a, bb = b; 
    xint res = (xint) ((aa * bb) % prime); 
    /* printf("%2d * %2d => %2d\n",aa,bb,(int)res); */
    return res;
}

/* A Xfield represents a xi-box in the multiplication matrix. It 
 *
 *  1)  holds a value (val)
 *  2)  knows where the mask of "forbidden bits" is stored (*oldmsk, *newmsk)
 *  3)  knows where the sum is kept (*sum, sum_weight)
 *  4)  knows where the reservoir for this row or column is (*res, res_weight)
 *
 * added extra feature:
 *  
 *  5)  "quantization of values", used to preserve profiles (quant)
 */

typedef struct {
    primeInfo *pi;
    xint val, 
        quant,
        *oldmsk, *newmsk, 
        *sum, 
        *res;
    int sum_weight, res_weight;
    int estat; 
} Xfield;

xint firstXdat(Xfield *X) {
    primeInfo *pi = X->pi; xint c, aux;
    X->val = *(X->res) / X->res_weight;
    X->val /= X->quant; 
    aux = *(X->oldmsk); aux /= X->quant;
    while (0 == (c=binomp(pi,X->val+aux,aux))) --(X->val);
    X->val *= X->quant;     
    *(X->newmsk) = *(X->oldmsk) + X->val; 
    *(X->res) -= X->val * X->res_weight; 
    *(X->sum) -= X->val * X->sum_weight; 
    return c;
}

xint nextXdat(Xfield *X) {
    primeInfo *pi = X->pi; xint c, nval, aux;;
    if (0 == (nval = X->val)) return 0;
    nval /= X->quant; 
    aux = *(X->oldmsk); aux /= X->quant; 
    while (0 == (c = binomp(pi,--(nval)+aux,aux))) ;
    nval *= X->quant;
    *(X->newmsk) = *(X->oldmsk) + nval; 
    *(X->sum) += (X->val - nval) * X->sum_weight; 
    *(X->res) += (X->val - nval) * X->res_weight; 
    X->val = nval;
    return c;
}

Xfield xfPA[NALG+1][NALG+1], xfAP[NALG+1][NALG+1];
xint msk[NALG+1][NALG+1], sum[NALG+1][NALG+1];
int emsk[NALG+1], esum[NALG+1];

void handlePArow(multArgs *ma, int row, xint coeff);

void handlePABox(multArgs *ma, int row, int col, xint coeff) {
    xint c;
    int eval = 1 << (col - 1); /* value of the exterior component */
    int sgn = 0;
    if ((0 != xfPA[row][col].estat) && 
        (*(xfPA[row][col].res) >= xfPA[row][col].res_weight) && 
        (0 == ((eval<<row) & emsk[row])) && (0 == (eval & esum[row]))) {
        *(xfPA[row][col].res) -= xfPA[row][col].res_weight;
        sgn = SIGNFUNC(emsk[row], (eval<<row)) + SIGNFUNC(esum[row], eval);
        emsk[row] |= (eval<<row); esum[row] |= eval;
    } else eval = 0;
    do {
        if (0 != (c = firstXdat(&(xfPA[row][col]))))
            do {
                xint prime = ma->pi->prime ;
                if (0 != (sgn & 1)) c = prime - c;
                if (col>1) 
                    handlePABox(ma, row, col-1, XINTMULT(coeff, c, prime));
                else 
                    handlePArow(ma, row-1, XINTMULT(coeff, c, prime));
            } while (0 != (c = nextXdat(&(xfPA[row][col]))));
        if (!eval) return;
        /* reset exterior bit */
        *(xfPA[row][col].res) += xfPA[row][col].res_weight;
        emsk[row] ^= (eval<<row); esum[row] ^= eval;
        eval = 0; sgn = 0;      
    } while (1);
}

void handlePArow(multArgs *ma, int row, xint coeff) {
    int i,k; xint c; 
    primeInfo *pi = ma->pi; xint prime = pi->prime;
     /* clear exterior field for this row */
    emsk[row] = emsk[row+1];
    esum[row] = esum[row+1];
    if (0 != row) {
        handlePABox(ma, row, NALG-row, coeff);
        return;
    }
    /* fetch summand */
    for (k=0;k<ma->s->num;k++) {
        mono res; /* summand of the result */
        mono *s = &(ma->s->dat[k]); /* second factor */
        c = coeff;
        /* first check exterior part */
        if (esum[1] != (s->ext & esum[1])) continue;
        if (0 != ((s->ext ^ esum[1]) & emsk[1])) continue;
        res.ext = (s->ext ^ esum[1]) | emsk[1];
        if (0 != (1 & (SIGNFUNC(emsk[1], s->ext ^ esum[1]) 
                       + SIGNFUNC(esum[1], s->ext ^ esum[1])))) 
            c = prime - c;
        /* now check reduced part */
        for (i=NALG;c && i--;) {
            xint aux, aux2;
            aux  = s->dat[i] + sum[0][i+1];
            aux2 = msk[1][i]; 
            if ((0 > (res.dat[i] = aux + aux2)) && ma->sIsPos) {
                c = 0;
            } else { 
                c = XINTMULT(c, binomp(pi, res.dat[i], aux), prime);
            }        
        }
        if (0 == c) continue;
        res.coeff = XINTMULT(c, s->coeff, prime);
        res.id    = s->id; /* should this be done in the callback function ? */
        ma->multCB(ma->clientData, &res);
    }
}

void workPAchain(multArgs *ma, mono *m) {
    int i;
    /* clear matrices */
    memset(msk, 0, sizeof(xint)*(NALG+1)*(NALG+1));
    memset(sum, 0, sizeof(xint)*(NALG+1)*(NALG+1));
    /* initialize oldmsk, sum, res*/
    for (i=NALG;i--;) { sum[0][i+1]=0; msk[i+1][0]=m->dat[i]; }
    emsk[NALG] = m->ext; esum[NALG] = 0;
    handlePArow(ma, NALG-1, m->coeff);
}

void initxfPA(multArgs *MA) {
    primeInfo *pi = MA->pi;
    int i,j;
    for (i=1;i<NALG;i++) {
        for (j=1;j<NALG;j++) {
            Xfield *xf = &(xfPA[i][j]);
            xf->pi = pi; 
            if (NULL == MA->profile) { 
                xf->quant = 1;
                xf->estat = 1;
            } else {
                xf->quant = pi->primpows[MA->profile->dat[i+j-1]];
                xf->estat = 
                    (0 == (MA->profile->ext & (1 << (i+j-1)))) ? 1 : 0;  
            }
            xf->oldmsk = &(msk[i+1][j-1]); 
            xf->newmsk = &(msk[i][j]);
            xf->res    = &(msk[i][0]); xf->res_weight = pi->primpows[j];
            xf->sum    = &(sum[0][j]); xf->sum_weight = 1;
        }
    }
}

void initxfAP(multArgs *MA) {
    primeInfo *pi = MA->pi;
    int i,j;
    for (i=1;i<NALG;i++) {
        for (j=1;j<NALG;j++) {
            Xfield *xf = &(xfAP[i][j]);
            xf->pi = pi; 
            if (NULL == MA->profile) { 
                xf->quant = 1;
                xf->estat = 1;
            } else {
                xf->quant = pi->primpows[MA->profile->dat[i+j-1]];
                xf->estat = 
                    (0 == (MA->profile->ext & (1 << (i+j-1)))) ? 1 : 0;  
            }
            xf->oldmsk = &(msk[i-1][j+1]); 
            xf->newmsk = &(msk[i][j]);
            xf->res    = &(msk[0][j]); xf->res_weight = 1; 
            xf->sum    = &(sum[i][0]); xf->sum_weight = pi->primpows[j];
        }
    }
}


void multPosAny(multArgs *MA, mono *f) {
    initxfPA(MA);
    workPAchain(MA, f);
}

/* same again: this time for any times pos */

void handleAPcol(multArgs *ma, int col, xint coeff);

void handleAPBox(multArgs *ma, int row, int col, xint coeff) {
    xint c;
    int emval = 2 << (row - 1);  /* mask version of esval */
    int sgn;
    if ((0 != xfAP[row][col].estat) && 
        (0 != (1 & emsk[col])) &&
        (0 == (emval & emsk[col]))) { 
        sgn = SIGNFUNC(1 | emval, 1 ^ emsk[col]);
        emsk[col] ^= 1 | emval;
        *(xfAP[row][col].sum) -= xfAP[row][col].sum_weight; 
    } else {
        emval = 0; sgn = 0;
    }
    do {
        if (0 != (c = firstXdat(&(xfAP[row][col]))))
            do {
                xint prime = ma->pi->prime ;
                if (1 & sgn) c = prime-c;
                if (row>1) 
                    handleAPBox(ma, row-1, col, CINTMULT(coeff, c, prime));
                else 
                    handleAPcol(ma, col-1, CINTMULT(coeff, c, prime));
            } while (0 != (c = nextXdat(&(xfAP[row][col]))));
        if (!emval) return;
        /* reset exterior fields */
        emsk[col] ^= 1 | emval;
        *(xfAP[row][col].sum) += xfAP[row][col].sum_weight; 
        sgn = 0; emval = 0;
    } while (1);
}

void handleAPcol(multArgs *ma, int col, xint coeff) {
    int i,k; xint c; 
    primeInfo *pi = ma->pi; xint prime = pi->prime;
    if (0 != col) {
        emsk[col] = (emsk[col+1] << 1) 
            | (1 & (esum[0] >> (col - 1))); 
        handleAPBox(ma, NALG-col, col, coeff);
        return;
    }
    /* fetch summand */
    for (k=0;k<ma->f->num;k++) {
        mono res; /* summand of the result */
        mono *f = &(ma->f->dat[k]); /* first factor */
        c = coeff;
        /* check exterior component */
        if (0 != (emsk[1] & f->ext)) continue;
        if (0 != (1 & SIGNFUNC(f->ext, emsk[1]))) c = prime-c;
        /* check reduced component */
        for (i=NALG;c && i--;) {
            xint aux, aux2;
            aux  = f->dat[i] + sum[i+1][0];
            aux2 = msk[i][1]; 
            res.dat[i] = aux + aux2;
            c = XINTMULT(c, binomp(pi, res.dat[i], aux), prime);
        }
        if (0 == c) continue;
        res.coeff = XINTMULT(c, f->coeff, prime);
        res.ext = f->ext | emsk[1];
        res.id    = f->id; /* should this be done in the callback function ? */
        ma->multCB(ma->clientData, &res);
    }
}

void workAPchain(multArgs *ma, mono *m) {
    int i;
    /* clear matrices */
    memset(msk, 0, sizeof(xint)*(NALG+1)*(NALG+1));
    memset(sum, 0, sizeof(xint)*(NALG+1)*(NALG+1));
    /* initialize oldmsk, sum, res*/
    for (i=NALG;i--;) { sum[i+1][0]=0; msk[0][i+1]=m->dat[i]; }
    emsk[NALG] = 0; esum[0] = m->ext;
    handleAPcol(ma, NALG-1, m->coeff);
}

void multAnyPos(multArgs *MA, mono *s) {
    initxfAP(MA);
    workAPchain(MA, s);
}
