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

#define POLYC

#include <stdlib.h>
#include <string.h>
#include "poly.h"
#include "common.h"

#define LOGSTD(msg) if (0) printf("stdpoly::%s\n", msg) 

/**** extended monomials ***********************************************************/

int exmoGetLen(exmo *e) {
    int pad = exmoGetPad(e), j;
    for (j=NALG;j--;) 
        if (e->dat[j] != pad) 
            return (j+1);
    return 0;
}

int exmoGetPad(exmo *e) {
    return (e->dat[0]<0) ? -1 : 0;
}

void copyExmo(exmo *dest, exmo *src) {
    memcpy(dest,src,sizeof(exmo));
}

void shiftExmo(exmo *e, const exmo *s, int flags) {
    int i;
    for (i=NALG;i--;) 
        e->dat[i] += s->dat[i];
    /* TODO: signs not yet implemented */
    e->ext ^= s->ext;
}

void reflectExmo(exmo *e) {
    int i;
    for (i=NALG;i--;) e->dat[i] = -1 - e->dat[i];
    e->ext = -1 - e->ext;
}

#define COMPRET(x,y) if (0 != (diff = ((x)-(y)))) return diff;
int compareExmo(const void *aa, const void *bb) {
    int diff, i;
    const exmo *a = (const exmo *) aa;
    const exmo *b = (const exmo *) bb;
    COMPRET(a->gen,b->gen);
    COMPRET(a->ext,b->ext);
    for (i=0;i<NALG;i++)
        COMPRET(a->dat[i],b->dat[i]);
    return 0;
}

/**** generic polynomials *********************************************************/

#define CALLIFNONZERO1(func,arg1) \
if (NULL != (func)) (func)(arg1);

int PLgetLength(polyType *type, void *poly) {
    return (type->getLength)(poly);
}

void PLfree(polyType *type, void *poly) { 
    CALLIFNONZERO1(type->free,poly); 
}

int PLclear(polyType *type, void *poly) { 
    CALLIFNONZERO1(type->clear,poly) else return FAILIMPOSSIBLE;
    return SUCCESS;
}

void *PLcreate(polyType *type) {
    void *res = NULL;
    if (NULL != type->createCopy) 
        res = (type->createCopy)(NULL);
    return res;
}

int PLcancel(polyType *type, void *poly, int modulo) { 
    if (NULL != type->cancel) {
        (type->cancel)(poly,modulo);
        return SUCCESS;
    }
    return FAILIMPOSSIBLE;
}

int PLgetExmo(polyType *type, void *self, exmo *ex, int index) {
    exmo *aux;
    if (NULL != type->getExmo) return (type->getExmo)(self, ex, index);
    if (NULL != type->getExmoPtr) 
        if (SUCCESS == (type->getExmoPtr)(self, &aux, index)) {
            copyExmo(ex,aux);
            return SUCCESS;
        }
    return FAILIMPOSSIBLE;
}

int PLappendExmo(polyType *dtp, void *dst, exmo *e) {
    if (NULL != dtp->appendExmo) return (dtp->appendExmo)(dst,e);
    return FAILIMPOSSIBLE;
}

int PLappendPoly(polyType *dtp, void *dst, 
                 polyType *stp, void *src,     
                 const exmo *shift,
                 int flags,
                 int scale, int modulo) {
    exmo e; int i, len;
    if ((dtp == src) && (NULL != dtp->appendPoly))
        return (dtp->appendPoly)(dst,src,shift,flags,scale,modulo);
    len = (stp->getLength)(src);
    if (modulo) scale %= modulo;
    for (i=0;i<len;i++) {
        if (SUCCESS != PLgetExmo(stp,src,&e,i)) 
            return FAILIMPOSSIBLE;
        if (NULL != shift) shiftExmo(&e,shift,flags);
        e.coeff *= scale; if (modulo) e.coeff %= modulo;
        if (SUCCESS != PLappendExmo(dtp,dst,&e)) 
            return FAILIMPOSSIBLE;
    }
    return SUCCESS;
}

/**** standard polynomial type ****************************************************/

/* The naive standard implementation of polynomials is as an array 
 * of exmo. This is represented by the stp structure. */

typedef struct {
    int num, nalloc;
    exmo *dat;
} stp;

void *stdCreateCopy(void *src) {
    stp *s = (stp *) src, *n;
    LOGSTD("CreateCopy");
    if (NULL == (n = (stp *) malloc(sizeof(stp)))) return NULL;
    if (NULL == s) {
        n->num = n->nalloc = 0; n->dat = NULL;
        return n;
    }
    if (NULL == (n->dat = (exmo *) malloc(sizeof(exmo) * s->num))) {
        free(n); return NULL; 
    }
    n->num = n->nalloc = s->num;
    memcpy(n->dat,s->dat,sizeof(exmo) * s->num);
    return n;
}

void stdFree(void *self) { 
    stp *s = (stp *) self;
    LOGSTD("Free");
    if (s->nalloc) { s->nalloc = s->num = 0; free(s->dat); }
    free(s);
}

void stdSwallow(void *self, void *other) { 
    stp *s = (stp *) self;
    stp *o = (stp *) other;
    LOGSTD("Swallow");
    if (s == o) return;
    stdFree(self); 
    memcpy(s,o,sizeof(stp));
    o->num = o->nalloc = 0; o->dat = NULL;
}

void stdClear(void *self) {
    stp *s = (stp *) self;
    LOGSTD("Clear");
    s->num = 0; 
}

int stdRealloc(void *self, int nalloc) {
    stp *s = (stp *) self;
    exmo *ndat;
    LOGSTD("Realloc");
    if (nalloc < s->num) nalloc = s->num;
    if (NULL == (ndat = realloc(s->dat,sizeof(exmo) * nalloc))) 
        return FAILMEM;
    s->dat = ndat; s->nalloc = nalloc;
    return SUCCESS;
}

void stdSort(void *self) {
    stp *s = (stp *) self;
    LOGSTD("Sort");
    qsort(s->dat,s->num,sizeof(exmo),compareExmo);
}

void stdCancel(void *self, int mod) {
    stp *s = (stp *) self;
    int i,j,k; double d;
    LOGSTD("Cancel");
    stdSort(self);
    for (k=i=0,j=1;j<=s->num;) 
        if ((j<s->num) && (0==compareExmo(&(s->dat[i]),&(s->dat[j])))) {
            s->dat[i].coeff += s->dat[j].coeff;
            if (mod) s->dat[i].coeff %= mod;
            j++;
        } else {
            if (mod) s->dat[i].coeff %= mod;
            if (0 != s->dat[i].coeff) {
                if (k != i) copyExmo(&(s->dat[k]),&(s->dat[i]));
                k++;
            }
            i=j; j=i+1;
        }
    s->num = k;
    if (s->nalloc) { 
        d = s->num; d /= s->nalloc; 
        if (0.8 > d) stdRealloc(self, s->num * 1.1);
    } 
}

int stdCompare(void *pol1, void *pol2, int *res) {
    stp *s1 = (stp *) pol1;
    stp *s2 = (stp *) pol2;
    LOGSTD("Compare");
    stdCancel(pol1,0); stdCancel(pol2,0);
    if (s1->num != s2->num) { 
        *res = s1->num - s2->num;
        return SUCCESS;
    }
    *res = memcmp(s1->dat,s2->dat,sizeof(exmo) * s1->num);
    return SUCCESS;
}

void stdReflect(void *self) {
    stp *s = (stp *) self;
    int i;
    LOGSTD("Reflect");
    for (i=0;i<s->num;i++) 
        reflectExmo(&(s->dat[i]));
}

void stdShift(void *self, const exmo *ex, int flags) {
    stp *s = (stp *) self;
    int i;
    LOGSTD("Reflect");
    for (i=0;i<s->num;i++) 
        shiftExmo(&(s->dat[i]), ex, flags);
}

int stdGetExmoPtr(void *self, exmo **ptr, int idx) {
    stp *s = (stp *) self;
    LOGSTD("GetExmoPtr");
    if ((idx < 0) || (idx >= s->num)) return FAILIMPOSSIBLE;
    *ptr = &(s->dat[idx]);
    return SUCCESS;
} 

int stdGetLength(void *self) {
    stp *s = (stp *) self;
    LOGSTD("GetLength");
    return s->num;
}

int stdAppendExmo(void *self, exmo *ex) {
    stp *s = (stp *) self;
    LOGSTD("AppendExmo");
    if (s->num == s->nalloc) {
        int aux = s->nalloc + 10;
        if (SUCCESS != stdRealloc(self, s->nalloc + ((aux > 200) ? 200: aux)))
            return FAILMEM;
    }
    copyExmo(&(s->dat[s->num++]),ex);
    return SUCCESS;
}

int stdScaleMod(void *self, int scale, int modulo) {
    stp *s = (stp *) self;
    int i;
    LOGSTD("ScaleMod");
    if (modulo) scale %= modulo;
    for (i=0;i<s->num;i++) {
        exmo *e = &(s->dat[i]);
        e->coeff *= scale; if (modulo) e->coeff %= modulo;
    }
    return SUCCESS;
}

/* GCC extension alert! (Designated Initializers) 
 *
 * see http://gcc.gnu.org/onlinedocs/gcc-3.2.3/gcc/Designated-Inits.html */

struct polyType stdPolyType = {
    .createCopy = &stdCreateCopy,
    .free       = &stdFree,
    .swallow    = &stdSwallow,
    .clear      = &stdClear,
    .cancel     = &stdCancel,
    .compare    = &stdCompare,
    .reflect    = &stdReflect,
    .getExmoPtr = &stdGetExmoPtr,
    .getLength  = &stdGetLength,
    .appendExmo = &stdAppendExmo,
    .scaleMod   = &stdScaleMod,
    .shift      = &stdShift
};

void *PLcreateStdCopy(polyType *type, void *poly) {
    return PLcreateCopy(stdpoly,type,poly);
}

void *PLcreateCopy(polyType *newtype, polyType *type, void *poly) {
    stp *res;
    if (newtype == type) 
        return (newtype->createCopy)(poly);
    if (NULL == (res = (newtype->createCopy)(NULL)))
        return NULL;
    if (SUCCESS != PLappendPoly(newtype,res,type,poly,NULL,0,1,0)) {
        (newtype->free)(res); return NULL;
    }
    return res;
}

int PLcompare(polyType *tp1, void *pol1, polyType *tp2, void *pol2, int *res) {
    void *st1, *st2;
    int rcode;
    if ((tp1 == tp2) && (NULL != tp1->compare)) 
        return (tp1->compare)(pol1,pol2,res);
    /* convert both to stdpoly */
    if (stdpoly == tp1) st1 = pol1;
    else st1 = PLcreateCopy(stdpoly,tp1,pol1);
    if (stdpoly == tp2) st2 = pol2;
    else st2 = PLcreateCopy(stdpoly,tp2,pol2);
    rcode = (stdpoly->compare)(st1,st2,res);
    if (st1 != pol1) PLfree(stdpoly,st1);
    if (st2 != pol2) PLfree(stdpoly,st2);
    return rcode;
}

int PLposMultiply(polyType **rtp, void **res,
                  polyType *fftp, void *ff,
                  polyType *sftp, void *sf, int mod) {
    exmo aux; int i, len, rc;
    *rtp = stdpoly; 
    *res = stdCreateCopy(NULL);
    len = PLgetLength(sftp,sf);
    for (i=0;i<len;i++) {
        if (SUCCESS != (rc = PLgetExmo(sftp,sf,&aux,i))) {
            PLfree(*rtp,*res); return rc;
        }
        if (SUCCESS != (rc = PLappendPoly(*rtp,*res,
                                          fftp,ff,
                                          &aux,ADJUSTSIGNS,
                                          aux.coeff,mod))) {
            PLfree(*rtp,*res); return rc;
        }
    }
    PLcancel(*rtp,*res,mod);
    return SUCCESS;  
}

int PLnegMultiply(polyType **rtp, void **res,
                  polyType *fftp, void *ff,
                  polyType *sftp, void *sf, int mod) {

    
    return FAILIMPOSSIBLE;
}

int PLsteenrodMultiply(polyType **rtp, void **res,
                       polyType *fftp, void *ff,
                       polyType *sftp, void *sf, int mod) {
    
    return FAILIMPOSSIBLE;
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

#endif
