/*
 * Steenrod algebra multiplication routine
 *
 * Copyright (C) 2004-2018 Christian Nassau <nassau@nullhomotopie.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define MULTC_INCLUDES

#include "mult.h"
#include <string.h>

#ifdef USESSE2
#  include "ssedefs.h"
#endif

static inline xint XINTMULT(xint a, xint b, xint prime) {
    int aa = a, bb = b;
    xint res = (xint) ((aa * bb) % prime);
    /* printf("%2d * %2d => %2d\n",aa,bb,(int)res); */
    return res;
}

/*** enumeration of xi fields */

xint firstXdat(Xfield *X, const primeInfo *pi) {
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

xint nextXdat(Xfield *X, const primeInfo *pi) {
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

void zeroXdat(Xfield *X) { X->val = 0; *(X->newmsk) = *(X->oldmsk); }

/*** standard fetch functions */

/* In stdFetchFuncSF our business is this:
 *
 *  1) go through all summands of the second factor
 *  2) check if one gets a summand of the product for this summand
 *     and the current multiplication matrix
 *  3) if yes then invoke the stdSummandFunc for this summand. */

#ifndef USESSE2
#  define stdFetchFuncSFNoSSE stdFetchFuncSF
#endif

void stdFetchFuncSFNoSSE(struct multArgs *ma, int coeff) {
    const exmo *sfx; int idx, i; cint c;
    const int prime = ma->pi->prime;
    const primeInfo * const pi = ma->pi;
    const int proext = (NULL == ma->profile) ? 0 : ma->profile->ext;
    for (idx = 0; SUCCESS == (ma->getExmoSF)(ma,SECOND_FACTOR,&sfx,idx); idx++) {
        exmo res; int hlp1;
        c = coeff;
        /* first check exterior part */
        if (ma->esum[1] != (sfx->ext & ma->esum[1])) continue;
        hlp1 = (sfx->ext ^ ma->esum[1]);
        if (0 != (hlp1 & proext)) continue;
        if (0 != (hlp1 & ma->emsk[1])) continue;
        /* now check reduced part */
        for (i=NALG;c && i--;) {
            xint aux, aux2;
            aux  = sfx->r.dat[i] + ma->sum[0][i+1];
            if (NULL != ma->profile)
                if (0 != (aux % (ma->profile->r.dat[i]))) {
                    c = 0;
                    break;
                }
            aux2 = ma->msk[1][i];
            if ((0 > (res.r.dat[i] = aux + aux2)) && ma->sfIsPos) {
                c = 0;
            } else {
                c = XINTMULT(c, binomp(pi, res.r.dat[i], aux), prime);
            }
        }
        if (0 == c) continue;
        res.ext = hlp1 | ma->emsk[1];
        if (0 != (1 & (SIGNFUNC(ma->emsk[1], hlp1)
                       + SIGNFUNC(ma->esum[1], hlp1))))
            c = prime - c;
        res.coeff = XINTMULT(c, sfx->coeff, prime);
        res.gen   = sfx->gen; /* should this be done in the callback function ? */
        /* invoke callback */
        (ma->stdSummandFunc)(ma, &res);
    }
}

#ifdef USESSE2

/* here comes the sse2 variant */

short inv2(short x) {
    unsigned int fac = 1-x, prod = 1+fac;
    while (fac) {
        prod *= 1 + (fac*=fac);
    }
    return prod;
}

void stdFetchFuncSF(struct multArgs *ma, int coeff) {
    const exmo *sfx; int idx; cint c;
    const int prime = ma->pi->prime;
    const primeInfo * const pi = ma->pi;
    const int proext = (NULL == ma->profile) ? 0 : ma->profile->ext;

    const __m128i masum = _mm_setr_epi16(ma->sum[0][1],
                                        ma->sum[0][2],
                                        ma->sum[0][3],
                                        ma->sum[0][4],
                                        ma->sum[0][5],
                                        ma->sum[0][6],
                                        ma->sum[0][7],
                                        ma->sum[0][8]);

    const __m128i mamsk = _mm_setr_epi16(ma->msk[1][0],
                                        ma->msk[1][1],
                                        ma->msk[1][2],
                                        ma->msk[1][3],
                                        ma->msk[1][4],
                                        ma->msk[1][5],
                                        ma->msk[1][6],
                                        ma->msk[1][7]);

    const __m128i zero = _mm_setzero_si128();

    const __m128i prfmsk = (prime & 1) ?
        (/* prime is odd => use 2-adic inverses */
            (NULL != ma->profile) ?
            _mm_setr_epi16(inv2(ma->profile->r.dat[0]),
                          inv2(ma->profile->r.dat[1]),
                          inv2(ma->profile->r.dat[2]),
                          inv2(ma->profile->r.dat[3]),
                          inv2(ma->profile->r.dat[4]),
                          inv2(ma->profile->r.dat[5]),
                          inv2(ma->profile->r.dat[6]),
                          inv2(ma->profile->r.dat[7])) :
            _mm_set1_epi16(1)) :
        (/* prime = 2 */
            (NULL != ma->profile) ?
            _mm_setr_epi16(ma->profile->r.dat[0]-1,
                          ma->profile->r.dat[1]-1,
                          ma->profile->r.dat[2]-1,
                          ma->profile->r.dat[3]-1,
                          ma->profile->r.dat[4]-1,
                          ma->profile->r.dat[5]-1,
                          ma->profile->r.dat[6]-1,
                          ma->profile->r.dat[7]-1) :
            _mm_set1_epi16(0)
      );

    PRINTMSG(" ==== stdFetchFuncSF (sse version) ==== ");
    PRINTEPI16(masum);
    PRINTEPI16(mamsk);
    PRINTEPI16(prfmsk);
    for (idx = 0; SUCCESS == (ma->getExmoSF)(ma,SECOND_FACTOR,&sfx,idx); idx++) {
        exmo res; int hlp1;
        PRINTEPI16(sfx->r.ssedat);

        c = coeff;
        /* first check exterior part */
        if (ma->esum[1] != (sfx->ext & ma->esum[1])) continue;
        hlp1 = (sfx->ext ^ ma->esum[1]);
        if (0 != (hlp1 & proext)) continue;
        if (0 != (hlp1 & ma->emsk[1])) continue;

        /* now check reduced part */
        {
            __m128i aux = _mm_add_epi16(sfx->r.ssedat,masum);
            __m128i final;

            PRINTEPI16(aux);

            if ((prime & 1)) {
                /* the prime is odd: we multiply by 2-adic inverse
                 * and check the size of the result */
                if (ma->sfIsPos) {
                    __m128i x = _mm_mullo_epi16(aux,prfmsk);
                    __m128i y = _mm_cmpgt_epi16(x,aux);
                    __m128i z = _mm_cmpgt_epi16(zero,x);
                    if (_mm_movemask_epi8(y) || _mm_movemask_epi8(z))
                        continue;
                } else {
                    __m128i x = _mm_mullo_epi16(aux,prfmsk);
                    __m128i y = _mm_cmpgt_epi16(aux,x);
                    __m128i z = _mm_cmpgt_epi16(x,zero);
                    if (_mm_movemask_epi8(y) || _mm_movemask_epi8(z))
                        continue;
                }
            } else {
                __m128i x = _mm_and_si128(prfmsk,aux); /* must be zero */
                if (0xffff ^ _mm_movemask_epi8(_mm_cmpeq_epi16(zero,x)))
                    continue;
            }

            final = _mm_add_epi16(aux,mamsk);
            PRINTEPI16(final);

            if (_mm_movemask_epi8(_mm_cmpgt_epi16(zero,aux)) && ma->sfIsPos)
                continue;

            c = XINTMULT(c, binompsse(pi, final, aux), prime);

            res.r.ssedat = final;
        }
        if (0 == c) continue;
        res.ext = hlp1 | ma->emsk[1];
        if (0 != (1 & (SIGNFUNC(ma->emsk[1], hlp1)
                       + SIGNFUNC(ma->esum[1], hlp1))))
            c = prime - c;
        res.coeff = XINTMULT(c, sfx->coeff, prime);
        res.gen   = sfx->gen; /* should this be done in the callback function ? */
        /* invoke callback */
        (ma->stdSummandFunc)(ma, &res);
    }

    PRINTMSG(" === done === ");
}

#endif /* USESSE2 */

/* The same in the AP case */

void stdFetchFuncFF(struct multArgs *ma, int coeff) {
    const exmo *ffx; int idx, i; cint c;
    int prime = ma->pi->prime;
    primeInfo *pi = ma->pi;
    int proext = (NULL == ma->profile) ? 0 : ma->profile->ext;
    for (idx = 0; SUCCESS == (ma->getExmoFF)(ma,FIRST_FACTOR,&ffx,idx); idx++) {
        exmo res;
        c = coeff;
        /* first check exterior part */
        if (0 != (ma->emsk[1] & proext)) continue;
        if (0 != (ma->emsk[1] & ffx->ext)) continue;
        if (0 != (1 & SIGNFUNC(ffx->ext, ma->emsk[1]))) c = prime-c;
        /* check reduced component */
        for (i=NALG;c && i--;) {
            xint aux, aux2;
            aux  = ffx->r.dat[i] + ma->sum[i+1][0];
            aux2 = ma->msk[i][1];
            if (NULL != ma->profile)
                if (0 != ((aux2 + ma->sum[i+1][0]) % ma->profile->r.dat[i])) {
                    c = 0;
                    break;
                }
            res.r.dat[i] = aux + aux2;
            c = XINTMULT(c, binomp(pi, res.r.dat[i], aux), prime);
        }
        if (0 == c) continue;
        res.coeff = XINTMULT(c, ffx->coeff, prime);
        res.ext = ffx->ext | ma->emsk[1];
        res.gen = ma->sfid; /* shouldn't this be done in the callback function ? */
        (ma->stdSummandFunc)(ma, &res);
    }
}

int stdGetExmoFunc(multArgs *ma, int factor, const exmo **ret, int idx) {
    polyType *ptp; void *pol; exmo *exm;
    if (0 != (FIRST_FACTOR & factor)) {
        *ret = exm = &(ma->fcdexmo);
        ptp = (polyType *) ma->ffdat;
        pol = ma->ffdat2;
    } else {
        *ret = exm = &(ma->scdexmo);
        ptp = (polyType *) ma->sfdat;
        pol = ma->sfdat2;
    }
    return PLgetExmo(ptp,pol,exm,idx);
}

int stdGetExmoFuncMotivic(multArgs *ma, int factor, const exmo **ret, int idx) {
  int ans = stdGetExmoFunc(ma,factor,ret,idx);
  /* we use the fact that *ret is owned by ma, hence not actually const */
  if(SUCCESS == ans) {
    motateExmo((exmo*)*ret);
    /* exmoLog("stdGetExmoFuncMotivic",*ret); */
  }
  return ans;
}

int stdGetSingleExmoFunc(multArgs *ma, int factor, const exmo **ret, int idx) {
    if (idx) return FAIL;
    if (0 != (FIRST_FACTOR & factor)) {
        *ret = (exmo *) ma->ffdat;
    } else {
        *ret = (exmo *) ma->sfdat;
    }
    return SUCCESS;
}

int stdGetSingleExmoFuncMotivic(multArgs *ma, int factor, const exmo **ret, int idx) {
  int ans = stdGetSingleExmoFunc(ma,factor,ret,idx);
  if(SUCCESS == ans) {
    exmo *emo = (exmo*)malloc(sizeof(exmo)); // FIXME &(ma->xcdexmo);
    memcpy(emo,*ret,sizeof(exmo));
    motateExmo(emo);
    *ret = emo;
    /* exmoLog("stdGetSingleExmoFuncMotivic",*ret); */
  }
  return ans;
}



/* initxfPA & initxfAP are helper functions for initMultargs */

void initxfPA(multArgs *MA) {
    primeInfo *pi = MA->pi;
    int i,j;
    for (i=1;i<NALG;i++) {
        for (j=1;j<NALG;j++) {
            Xfield *xf = &(MA->xfPA[i][j]);
            if (NULL == MA->profile) {
                xf->quant = 1;
                xf->estat = 1;
            } else {
                xf->quant = (i+j-1<NALG) ? MA->profile->r.dat[i+j-1] : 1;
                xf->estat =
                    (0 == (MA->profile->ext & (1 << (i+j-1)))) ? 1 : 0;
            }
            xf->oldmsk = &(MA->msk[i+1][j-1]);
            xf->newmsk = &(MA->msk[i][j]);
            xf->res    = &(MA->msk[i][0]); xf->res_weight = pi->primpows[j];
            xf->sum    = &(MA->sum[0][j]); xf->sum_weight = 1;
            xf->ext_weight = pi->primpows[j-1];
        }
    }
}

void initxfAP(multArgs *MA) {
    primeInfo *pi = MA->pi;
    int i,j;
    for (i=1;i<NALG;i++) {
        for (j=1;j<NALG;j++) {
            Xfield *xf = &(MA->xfAP[i][j]);
            if (NULL == MA->profile) {
                xf->quant = 1;
                xf->estat = 1;
            } else {
                xf->quant = (i+j-1<NALG) ? MA->profile->r.dat[i+j-1] : 1;
                xf->estat =
                    (0 == (MA->profile->ext & (1 << (i+j-1)))) ? 1 : 0;
            }
            xf->oldmsk = &(MA->msk[i-1][j+1]);
            xf->newmsk = &(MA->msk[i][j]);
            xf->res    = &(MA->msk[0][j]); xf->res_weight = 1;
            xf->sum    = &(MA->sum[i][0]); xf->sum_weight = pi->primpows[j];
            xf->ext_weight = pi->primpows[j-1];
        }
    }
}

void initMultargs(multArgs *ma, primeInfo *pi, exmo *profile) {
    ma->pi = pi;
    ma->profile = profile;
    ma->prime = pi->prime;
    initxfAP(ma);
    initxfPA(ma);
}

/***** ALL OF THIS NEEDS TO BE OPTIMIZED FURTHER ! *****/

/* The ugly details of PA multiplication... */

void handlePArow(multArgs *ma, int row, xint coeff);

void handlePABox(multArgs *ma, int row, int col, xint coeff) {
    xint c;
    int eval = 1 << (col - 1); /* value of the exterior component */
    int sgn = 0;
#if 0
    while (col > (1 + ma->sfMaxLength)) {
        zeroXdat(&(ma->xfPA[row][col]));
        if (col <= 1) {
            handlePArow(ma, row-1, coeff);
            return;
        }
        col--;
    }
#endif
    if ((0 != ma->xfPA[row][col].estat)
        && (*(ma->xfPA[row][col].res) >= ma->xfPA[row][col].ext_weight)
        && (0 == ((eval<<row) & ma->emsk[row])) && (0 == (eval & ma->esum[row]))) {
        *(ma->xfPA[row][col].res) -= ma->xfPA[row][col].ext_weight;
        sgn = SIGNFUNC(ma->emsk[row], (eval<<row)) + SIGNFUNC(ma->esum[row], eval);
        ma->emsk[row] |= (eval<<row); ma->esum[row] |= eval;
    } else eval = 0;
    do {
        if (col > (1 + ma->sfMaxLength)) {
            xint c = coeff;
            if (0 != (sgn & 1)) c = - c;
            zeroXdat(&(ma->xfPA[row][col]));
            if (col>1)
                handlePABox(ma, row, col-1, c);
            else
                handlePArow(ma, row-1, c);
        } else {
            if (0 != (c = firstXdat(&(ma->xfPA[row][col]),ma->pi)))
                do {
                    xint prime = ma->pi->prime ;
                    if (0 != (sgn & 1)) c = - c;
                    if (col>1)
                        handlePABox(ma, row, col-1, XINTMULT(coeff, c, prime));
                    else
                        handlePArow(ma, row-1, XINTMULT(coeff, c, prime));
                } while (0 != (c = nextXdat(&(ma->xfPA[row][col]),ma->pi)));
        }
        if (!eval) return;
        /* reset exterior bit */
        *(ma->xfPA[row][col].res) += ma->xfPA[row][col].ext_weight;
        ma->emsk[row] ^= (eval<<row); ma->esum[row] ^= eval;
        eval = 0; sgn = 0;
    } while (1);
}

void handlePArow(multArgs *ma, int row, xint coeff) {
     /* clear exterior field for this row */
    ma->emsk[row] = ma->emsk[row+1];
    ma->esum[row] = ma->esum[row+1];
    if (0 != row)
        handlePABox(ma, row, NALG-row, coeff);
    else
        (ma->fetchFuncSF)(ma,coeff);
}

/* More ugly details, now of AP multiplication... */

void handleAPcol(multArgs *ma, int col, xint coeff);

void handleAPBox(multArgs *ma, int row, int col, xint coeff) {
    xint c;
    int emval = 2 << (row - 1);  /* mask version of esval */
    int sgn = 0;
    while (row > (1 + ma->ffMaxLength)) {
        zeroXdat(&(ma->xfAP[row][col]));
        if (row <= 1) {
            handleAPcol(ma, col-1, coeff);
            return;
        }
        row--;
    }
    if ((0 != ma->xfAP[row][col].estat)
        && (0 != (1 & ma->emsk[col]))
        && (0 == (emval & ma->emsk[col]))) {
        sgn = SIGNFUNC(1 | emval, 1 ^ ma->emsk[col]);
        ma->emsk[col] ^= 1 | emval;
        *(ma->xfAP[row][col].sum) -= ma->xfAP[row][col].ext_weight;
    } else {
        emval = 0; sgn = 0;
    }
    do {
        if (0 != (c = firstXdat(&(ma->xfAP[row][col]),ma->pi)))
            do {
                xint prime = ma->pi->prime ;
                if (1 & sgn) c = prime-c;
                if (row>1)
                    handleAPBox(ma, row-1, col, CINTMULT(coeff, c, prime));
                else
                    handleAPcol(ma, col-1, CINTMULT(coeff, c, prime));
            } while (0 != (c = nextXdat(&(ma->xfAP[row][col]),ma->pi)));
        if (!emval) return;
        /* reset exterior fields */
        ma->emsk[col] ^= 1 | emval;
        *(ma->xfAP[row][col].sum) += ma->xfAP[row][col].ext_weight;
        sgn = 0; emval = 0;
    } while (1);
}

void handleAPcol(multArgs *ma, int col, xint coeff) {
    if (0 != col) {
        ma->emsk[col] = (ma->emsk[col+1] << 1)
            | (1 & (ma->esum[0] >> (col - 1)));
        handleAPBox(ma, MIN(NALG-col, NALG), col, coeff);
    } else
        (ma->fetchFuncFF)(ma,coeff);
}

/* workXYchain starts the computation */

void workPAchain(multArgs *ma) {
    int i, idx, inirow; const exmo *m;
    for (idx=0; SUCCESS == (ma->getExmoFF)(ma,FIRST_FACTOR,&m,idx); idx++) {
        /* clear matrices */
        memset(ma->msk, 0, sizeof(xint)*(NALG+1)*(NALG+1));
        memset(ma->sum, 0, sizeof(xint)*(NALG+1)*(NALG+1));
        ma->ffid = m->gen;
        /* initialize oldmsk, sum, res */
        for (i=NALG;i--;) { ma->sum[0][i+1]=0; ma->msk[i+1][0]=m->r.dat[i]; }
        inirow = 1 + ma->ffMaxLength;
        ma->emsk[inirow + 1] = m->ext; ma->esum[inirow + 1] = 0;
        handlePArow(ma, inirow, m->coeff);
    }
}

void workAPchain(multArgs *ma) {
    int i, idx, inicol; const exmo *m;
    for (idx=0; SUCCESS == (ma->getExmoSF)(ma,SECOND_FACTOR,&m,idx); idx++) {
        /* clear matrices */
        memset(ma->msk, 0, sizeof(xint)*(NALG+1)*(NALG+1));
        memset(ma->sum, 0, sizeof(xint)*(NALG+1)*(NALG+1));
        ma->sfid = m->gen;
        /* initialize oldmsk, sum, res*/
        for (i=NALG;i--;) { ma->sum[i+1][0]=0; ma->msk[0][i+1]=m->r.dat[i]; }
        inicol = 1 + ma->sfMaxLength;
        ma->emsk[1 + inicol] = 0; ma->esum[0] = m->ext;
        handleAPcol(ma, inicol, m->coeff);
    }
}

void stdAddSummandToPoly(struct multArgs *ma, const exmo *smd) {
    PLappendExmo(ma->resPolyType,ma->resPolyPtr, smd);
}

int stdAddProductToPoly(polyType *rtp, void *res,
                        polyType *ftp, void *ff,
                        polyType *stp, void *sf,
                        primeInfo *pi, const exmo *pro,
                        int fIsPos, int sIsPos) {
    multArgs ourMA, *ma = &ourMA;

    initMultargs(ma, pi, (exmo *) pro);

    ma->ffIsPos = fIsPos;
    ma->sfIsPos = sIsPos;

    ma->ffMaxLength = PLgetMaxRedLength(ftp, ff);
    ma->sfMaxLength = PLgetMaxRedLength(stp, sf);

    ma->ffMaxLength = MIN(ma->ffMaxLength, NALG-2);
    ma->sfMaxLength = MIN(ma->sfMaxLength, NALG-2);

    ma->ffdat = ftp; ma->ffdat2 = ff;
    ma->getExmoFF = &stdGetExmoFunc;
    ma->fetchFuncFF = &stdFetchFuncFF;

    ma->sfdat = stp; ma->sfdat2 = sf;
    ma->getExmoSF = &stdGetExmoFunc;
    ma->fetchFuncSF = &stdFetchFuncSF;

    ma->resPolyType = rtp;
    ma->resPolyPtr = res;
    ma->stdSummandFunc = stdAddSummandToPoly;

    if (fIsPos)
        workPAchain(ma);
    else
        workAPchain(ma);

    multCount += PLgetNumsum(ftp, ff) * PLgetNumsum(stp, sf);

    return SUCCESS;
}

/* --- EBP variants -------------------------------------------------------- */


xint firstXdatEBP(multArgs *ma, Xfield *X, primeInfo *pi, int *collision) {
    xint c, aux;
    const int collisionAllowed = ma->collisionAllowed;
     X->val = *(X->res) / X->res_weight;
    X->val /= X->quant;
    aux = *(X->oldmsk); aux /= X->quant;
    do {
	while (0 == (c=binomp2(pi,X->val+aux,aux,collision))) --(X->val);
    } while (!collisionAllowed && *collision >= 0);
    X->val *= X->quant;
    *(X->newmsk) = *(X->oldmsk) + X->val;
    *(X->res) -= X->val * X->res_weight;
    *(X->sum) -= X->val * X->sum_weight;
    return c;
}

xint nextXdatEBP(multArgs *ma, Xfield *X, primeInfo *pi, int *collision) {
    xint c, nval, aux;
    const int collisionAllowed = ma->collisionAllowed;
    if (0 == (nval = X->val)) return 0;
    nval /= X->quant;
    aux = *(X->oldmsk); aux /= X->quant;
    do {
	while (0 == (c = binomp2(pi,--(nval)+aux,aux,collision))) ;
    } while (!collisionAllowed && *collision >= 0);
    nval *= X->quant;
    *(X->newmsk) = *(X->oldmsk) + nval;
    *(X->sum) += (X->val - nval) * X->sum_weight;
    *(X->res) += (X->val - nval) * X->res_weight;
    X->val = nval;
    return c;
}

void zeroXdatEBP(Xfield *X) { X->val = 0; *(X->newmsk) = *(X->oldmsk); }

void handlePArowEBP(multArgs *ma, int row, xint coeff);

void handlePABoxEBP(multArgs *ma, int row, int col, xint coeff) {
    xint c;
    int eval = 1 << (col - 1); /* value of the exterior component */
    int sgn = 0, collision;
    const int collisionIn = ma->collision, collisionAllowedIn = ma->collisionAllowed;
    if ((0 != ma->xfPA[row][col].estat)
        && (*(ma->xfPA[row][col].res) >= ma->xfPA[row][col].ext_weight)
        && (0 == ((eval<<row) & ma->emsk[row])) && (0 == (eval & ma->esum[row]))) {
        *(ma->xfPA[row][col].res) -= ma->xfPA[row][col].ext_weight;
        sgn = SIGNFUNC(ma->emsk[row], (eval<<row)) + SIGNFUNC(ma->esum[row], eval);
        ma->emsk[row] |= (eval<<row); ma->esum[row] |= eval;
    } else eval = 0;
    do {
        if (col > (1 + ma->sfMaxLength)) {
            xint c = coeff;
            if (0 != (sgn & 1)) c = - c;
            zeroXdatEBP(&(ma->xfPA[row][col]));
            if (col>1)
                handlePABoxEBP(ma, row, col-1, c);
            else
                handlePArowEBP(ma, row-1, c);
        } else {
            if (0 != (c = firstXdatEBP(ma,&(ma->xfPA[row][col]),ma->pi,&collision)))
                do {
                    xint prime2 = ma->pi->prime2, c2;
                    if (0 != (sgn & 1)) c2 = - c; else c2 = c;
                    if (col>1)
                        handlePABoxEBP(ma, row, col-1, XINTMULT(coeff, c2, prime2));
                    else
                        handlePArowEBP(ma, row-1, XINTMULT(coeff, c2, prime2));
                } while (0 != (c = nextXdatEBP(ma,&(ma->xfPA[row][col]),ma->pi,&collision)));
        }
        if (!eval) break;
        /* reset exterior bit */
        *(ma->xfPA[row][col].res) += ma->xfPA[row][col].ext_weight;
        ma->emsk[row] ^= (eval<<row); ma->esum[row] ^= eval;
        eval = 0; sgn = 0;
    } while (1);
    ma->collision = collisionIn;
    ma->collisionAllowed = collisionAllowedIn;
}

void handlePArowEBP(multArgs *ma, int row, xint coeff) {
     /* clear exterior field for this row */
    ma->emsk[row] = ma->emsk[row+1];
    ma->esum[row] = ma->esum[row+1];
    if (0 != row)
        handlePABoxEBP(ma, row, NALG-row, coeff);
    else
        (ma->fetchFuncSF)(ma,coeff);
}

void workPAchainEBP(multArgs *ma) {
    int i, idx, inirow; const exmo *m;
    for (idx=0; SUCCESS == (ma->getExmoFF)(ma,FIRST_FACTOR,&m,idx); idx++) {
        /* clear matrices */
        memset(ma->msk, 0, sizeof(xint)*(NALG+1)*(NALG+1));
        memset(ma->sum, 0, sizeof(xint)*(NALG+1)*(NALG+1));
        ma->ffid = m->gen;
        /* initialize oldmsk, sum, res */
        for (i=NALG;i--;) { ma->sum[0][i+1]=0; ma->msk[i+1][0]=m->r.dat[i]; }
        inirow = 1 + ma->ffMaxLength;
        ma->emsk[inirow + 1] = m->ext; ma->esum[inirow + 1] = 0;
        handlePArowEBP(ma, inirow, m->coeff);
    }
}

void stdFetchFuncSFEBP(struct multArgs *ma, int coeff) {
    const exmo *sfx; int idx, i; cint c;
    const int prime2 = ma->pi->prime2;
    primeInfo * pi = ma->pi;
    const int proext = (NULL == ma->profile) ? 0 : ma->profile->ext;
    int collision;
    for (idx = 0; SUCCESS == (ma->getExmoSF)(ma,SECOND_FACTOR,&sfx,idx); idx++) {
        exmo res; int hlp1;
        c = coeff;
        /* first check exterior part */
        if (ma->esum[1] != (sfx->ext & ma->esum[1])) continue;
        hlp1 = (sfx->ext ^ ma->esum[1]);
        if (0 != (hlp1 & proext)) continue;
        if (0 != (hlp1 & ma->emsk[1])) continue;
        /* now check reduced part */
        for (i=NALG;c && i--;) {
            xint aux, aux2;
            aux  = sfx->r.dat[i] + ma->sum[0][i+1];
            if (NULL != ma->profile)
                if (0 != (aux % (ma->profile->r.dat[i]))) {
                    c = 0;
                    break;
                }
            aux2 = ma->msk[1][i];
            if ((0 > (res.r.dat[i] = aux + aux2)) && ma->sfIsPos) {
                c = 0;
            } else {
                c = XINTMULT(c, binomp2(pi, res.r.dat[i], aux,&collision), prime2);
            }
        }
        if (0 == c) continue;
        res.ext = hlp1 | ma->emsk[1];
        if (0 != (1 & (SIGNFUNC(ma->emsk[1], hlp1)
                       + SIGNFUNC(ma->esum[1], hlp1))))
            c = prime2 - c;
        res.coeff = XINTMULT(c, sfx->coeff, prime2);
        res.gen   = sfx->gen; /* should this be done in the callback function ? */
        /* invoke callback */
        (ma->stdSummandFunc)(ma, &res);
    }
}

int stdAddProductToPolyEBP(polyType *rtp, void *res,
                        polyType *ftp, void *ff,
                        polyType *stp, void *sf,
                        primeInfo *pi) {
    multArgs ourMA, *ma = &ourMA;

    initMultargs(ma, pi, NULL);

    ma->ffIsPos = 1;
    ma->sfIsPos = 1;

    ma->collision = -1;

    ma->ffMaxLength = PLgetMaxRedLength(ftp, ff);
    ma->sfMaxLength = PLgetMaxRedLength(stp, sf);

    ma->ffMaxLength = MIN(ma->ffMaxLength, NALG-2);
    ma->sfMaxLength = MIN(ma->sfMaxLength, NALG-2);

    ma->ffdat = ftp; ma->ffdat2 = ff;
    ma->getExmoFF = &stdGetExmoFunc;
    ma->fetchFuncFF = &stdFetchFuncFF;

    ma->sfdat = stp; ma->sfdat2 = sf;
    ma->getExmoSF = &stdGetExmoFunc;
    ma->fetchFuncSF = &stdFetchFuncSFEBP;

    ma->resPolyType = rtp;
    ma->resPolyPtr = res;
    ma->stdSummandFunc = stdAddSummandToPoly;

    workPAchainEBP(ma);

    multCount += PLgetNumsum(ftp, ff) * PLgetNumsum(stp, sf);

    return SUCCESS;
}
