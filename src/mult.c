/*
 * Multiplication routine
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

#include "mult.h"

inline xint XINTMULT(xint a, xint b, xint prime) { 
    int aa = a, bb = b; 
    xint res = (xint) ((aa * bb) % prime); 
    /* printf("%2d * %2d => %2d\n",aa,bb,(int)res); */
    return res;
}

/*** enumeration of xi fields */

xint firstXdat(Xfield *X, primeInfo *pi) {
    xint c, aux;
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

xint nextXdat(Xfield *X, primeInfo *pi) {
    xint c, nval, aux;
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

/*** standard fetch functions */

/* In stdFetchFuncSF our business is this:
 *
 *  1) go through all summands of the second factor
 *  2) check if one gets a summand of the product for this summand 
 *     and the current multiplication matrix
 *  3) if yes then invoke the stdSummandFunc for this summand. */

void stdFetchFuncSF(struct multArgs *ma, int coeff) {
    const exmo *sfx; int idx, i; cint c; 
    int prime = ma->pi->prime; 
    primeInfo *pi = ma->pi;
    for (idx = 0; SUCCESS == ((ma->getExmoSF)(ma->sfdat, &sfx,idx)); idx++) {
        exmo res;
        c = coeff;
        /* first check exterior part */
        if (ma->esum[1] != (sfx->ext & ma->esum[1])) continue;
        if (0 != ((sfx->ext ^ ma->esum[1]) & ma->emsk[1])) continue;
        res.ext = (sfx->ext ^ ma->esum[1]) | ma->emsk[1];
        if (0 != (1 & (SIGNFUNC(ma->emsk[1], sfx->ext ^ ma->esum[1])
                       + SIGNFUNC(ma->esum[1], sfx->ext ^ ma->esum[1]))))
            c = prime - c;
        /* now check reduced part */
        for (i=NALG;c && i--;) {
            xint aux, aux2;
            aux  = sfx->dat[i] + ma->sum[0][i+1];
            aux2 = ma->msk[1][i];
            if ((0 > (res.dat[i] = aux + aux2)) && ma->sfIsPos) {
                c = 0;
            } else {
                c = XINTMULT(c, binomp(pi, res.dat[i], aux), prime);
            }
        }
        if (0 == c) continue;
        res.coeff = XINTMULT(c, sfx->coeff, prime);
        res.gen   = sfx->gen; /* should this be done in the callback function ? */
        /* invoke callback */
        (ma->stdSummandFunc)(ma, &res);
    }
}

/* The same in the AP case */

void stdFetchFuncFF(struct multArgs *ma, int coeff) {
    const exmo *ffx; int idx, i; cint c; 
    int prime = ma->pi->prime; 
    primeInfo *pi = ma->pi;
    for (idx = 0; SUCCESS == ((ma->getExmoFF)(ma->ffdat, &ffx,idx)); idx++) {
        exmo res;
        c = coeff;
        /* first check exterior part */
        if (0 != (ma->emsk[1] & ffx->ext)) continue;
        if (0 != (1 & SIGNFUNC(ffx->ext, ma->emsk[1]))) c = prime-c;
        /* check reduced component */
        for (i=NALG;c && i--;) {
            xint aux, aux2;
            aux  = ffx->dat[i] + ma->sum[i+1][0];
            aux2 = ma->msk[i][1];
            res.dat[i] = aux + aux2;
            c = XINTMULT(c, binomp(pi, res.dat[i], aux), prime);
        }
        if (0 == c) continue;
        res.coeff = XINTMULT(c, ffx->coeff, prime);
        res.ext = ffx->ext | ma->emsk[1];
        /* WRONG: really take gen id from first summand ??? */
        res.gen = ffx->gen; /* should this be done in the callback function ? */
        (ma->stdSummandFunc)(ma, &res);
    }
}

#if 0

/* old stuff follows below */

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

#endif
