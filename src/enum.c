/*
 * All about enumeration and sequence numbers
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

#define ENLOG 0

#define ENUMC

#include <string.h>
#include "enum.h"

enumerator *enmCreate(void) {
    enumerator *en;
    en = callox(1, sizeof(enumerator));
    return en;
}

void *copymem(void *src, size_t sz) {
    void *res;
    if (NULL == src) return NULL;
    if (NULL == (res = mallox(sz))) return NULL;
    memcpy(res, src, sz);
    return res;
}

enumerator *enmCopy(enumerator *src) {
    enumerator *en;
    int i;

    if (NULL == (en = enmCreate())) return NULL;
 
    /* copy all values... */
    memcpy(en, src, sizeof(enumerator));

    /* ... but clear pointers */
    en->genList = NULL;
    for (i=NALG+1;i--;) 
        en->dimtab[i] = en->seqtab[i] = NULL;

    en->efflist = NULL; 
    en->seqoff = NULL;

    /* now make private copies of these arrays */
    en->genList = (int *) copymem(src->genList, 4 * sizeof(int) * src->numgens);

    for (i=NALG+1;i--;) {
        en->dimtab[i] = (int *) copymem(src->dimtab[i], sizeof(int) * src->tablen);
        en->seqtab[i] = (int *) copymem(src->seqtab[i], sizeof(int) * src->tablen);
    }

    en->efflist = (effgen *) copymem(src->efflist, src->efflen * sizeof(effgen));
    en->seqoff = (int *) copymem(src->seqoff, src->efflen * sizeof(int)); 

    return en;
}

#define FREEPTR(p) { if (NULL != (p)) { freex(p); p = NULL; } }

void enmDestroySeqOff(enumerator *en) {
    FREEPTR(en->seqoff);
}

void enmDestroySeqtab(enumerator *en) {
    int i;      
#if 0  
    printf("enmDestroySeqtab en=%p\n",en);
    for (i=0;i<NALG+1;i++) {
        printf("  en(%p)->dimtab[%d]=%p\n",en,i,en->dimtab[i]);
        printf("  en(%p)->seqtab[%d]=%p\n",en,i,en->seqtab[i]);
    }
#endif
    for (i=0;i<NALG+1;i++) {
        FREEPTR(en->dimtab[i]);
        FREEPTR(en->seqtab[i]);
    }
    enmDestroySeqOff(en);
}

void enmDestroyEffList(enumerator *en) {
    FREEPTR(en->efflist);
    en->effalloc = en->efflen = 0;
    enmDestroySeqOff(en);
}

void enmDestroyGenList(enumerator *en) {
    FREEPTR(en->genList);
    en->numgens = 0;
    enmDestroyEffList(en);
}

void enmDestroy(enumerator *en) {
    en->pi = NULL;
    enmDestroyGenList(en);
    enmDestroySeqtab(en);
}

int enmReallocEfflist(enumerator *en, int size) {
    effgen *nptr;
    if (size < en->efflen) size = en->efflen;
    if (NULL == (nptr = (effgen *) reallox(en->efflist, size * sizeof(effgen))))
        return FAILMEM;
    en->effalloc = size;
    en->efflist = nptr;
    return SUCCESS;
}

int getMaxExterior(primeInfo *pi, exmo *alg, exmo *pro, int ideg) {
    int res=0, wrk=1<<NALG, i=NALG;
    while (wrk>>=1,i--)
        if ((NULL == alg) || (wrk == (wrk & alg->ext)))
            if ((NULL == pro) || (0 == (wrk & pro->ext)))
                if (pi->extdegs[i] <= ideg) {
                    res |= wrk; ideg -= pi->extdegs[i];
                }
    return res;
}

#define COMPRET(x,y) if (0 != (res = (x)-(y))) return res;
int compareEffgen(const void *a, const void *b) {
    const effgen *aa = (const effgen *) a;
    const effgen *bb = (const effgen *) b;
    int res;

    /* Note: in order to optimize the computation of differentials we
     * might later want to combine the computation of d(Q(E)P(R)g) for 
     * common Q(E)P(R) and varying g; therefore "ext" has to be more 
     * significant than "id". */

    COMPRET(aa->ext, bb->ext);
    COMPRET(aa->id,  bb->id);
    return 0;
}

void enmSortEfflist(enumerator *en) {
    qsort(en->efflist, en->efflen, sizeof(effgen), compareEffgen);
    if (ENLOG) printf("sorted %d effgens\n", en->efflen);
}

/* clip *ex to its signature with respect to the profile *prof */
void clipExmo(exmo *ex, exmo *prof) {
    int i;
    ex->ext &= prof->ext;
    for (i=NALG;i--;) {
        div_t dr = div(ex->dat[i], prof->dat[i]);
        if (dr.rem < 0) dr.rem += prof->dat[i];
        ex->dat[i] = dr.rem;
    }
}

void enmUpdateSigInfo(enumerator *en, exmo *sig, int *sideg, int *sedeg) {
    /* clip signature to profile and determine degrees */
    clipExmo(sig, &(en->profile));
    *sedeg = BITCOUNT(sig->ext);
    *sideg = exmoIdeg(en->pi, sig);
}

int enmAppendEffgen(enumerator *en, int id, int ext, int rrideg, int redeg) {
    effgen *gen;
    if (en->effalloc <= en->efflen) 
        if (SUCCESS != enmReallocEfflist(en, en->efflen+500)) {
            enmDestroyEffList(en);
            return FAILMEM;
        }
    gen = &(en->efflist[en->efflen++]);
    gen->id     = id;
    gen->ext    = ext;
    gen->rrideg = rrideg;
    if (gen->rrideg > en->maxrrideg) 
        en->maxrrideg = gen->rrideg;
    if (redeg > en->maxredeg)
        en->maxredeg = redeg;
    if (ENLOG) printf("  effgen id=%d ext=%d rrideg=%d\n",
                      id, ext, gen->rrideg);
    return SUCCESS;
}

int findExtsForGenerator(enumerator *en, int id, int rideg, int redeg) {
    int ext = getMaxExterior(en->pi, &(en->algebra), &(en->profile), rideg);
    int tpmo = en->pi->tpmo, proext = en->profile.ext; 
    if (1 && ENLOG) printf("maxExterior=%d (rideg=%d)\n", ext, rideg);
    for (;ext>=0;ext--) 
        if ((0 == (ext & proext)) && (BITCOUNT(ext) == redeg)) {
            int extideg = extdeg(en->pi, ext);
            if (extideg <= rideg) {
                int diffideg = rideg - extideg;
                if (0 == (diffideg % tpmo)) {
                    if (SUCCESS != enmAppendEffgen(en, id, ext, 
                                                   diffideg/tpmo, 
                                                   redeg))
                        return FAILMEM; 
                }
            }
        }
    return SUCCESS;
}

/* create list of effective generators for the given tridegree */
int enmRecreateEfflist(enumerator *en) {
    int i, *glp, tideg, tedeg, thdeg;
    if (NULL == en->pi) return FAILIMPOSSIBLE;

    /* if present: destroy old values */
    enmDestroyEffList(en);
    en->maxrrideg = 0; en->maxredeg = -1;

    /* find real tridegree coords */
    enmUpdateSigInfo(en, &(en->signature), &(en->sigideg), &(en->sigedeg));
    thdeg = en->hdeg;
    tedeg = en->edeg + (en->ispos ? (-en->sigedeg) : en->sigedeg);
    tideg = en->ideg + (en->ispos ? (-en->sigideg) : en->sigideg);
    if (ENLOG) printf("after signature correction: (sig: %d,%d) "
                      "trideg = (%d, %d, %d)\n", 
                      en->sigideg, en->sigedeg, tideg, tedeg, thdeg);

    /* go through all gens in the genlist and look for appropriate exteriors */
    for (i=0,glp=en->genList; i<en->numgens; i++,glp+=4) {
        int id = glp[0], ideg = glp[1], edeg = glp[2], hdeg = glp[3];
        int rideg = tideg - ideg, redeg = tedeg - edeg;
        if (ENLOG) printf("looking at generator with "
                          "id=%d ideg=%d edeg=%d hdeg=%d\n", id, ideg, edeg, hdeg);
        if (hdeg != en->hdeg) continue;
        if (en->ispos) {
            if ((redeg >= 0) && (rideg >= 0)) {
                if (SUCCESS != findExtsForGenerator(en, id, rideg, redeg))
                    return FAIL;
            } 
        } else {
            if ((redeg <= 0) && (rideg <= 0)) {
                if (SUCCESS != findExtsForGenerator(en, id, -rideg, -redeg))
                    return FAIL;
            } 
        }
    }

    /* realloc to minimal space */
    if (SUCCESS != enmReallocEfflist(en, 0)) {
        enmDestroyEffList(en);
        return FAILMEM;
    }
    enmSortEfflist(en);
    return SUCCESS;
}

/**** PUBLIC CONFIGURATION FUNCTIONS ****************************************/

int enmSetBasics(enumerator *en, primeInfo *pi, 
                 exmo *algebra, exmo *profile, int ispos) {
    int i;
    enmDestroyEffList(en);
    enmDestroySeqtab(en);
    en->pi = pi;
    en->ispos = ispos;

    if (NULL == algebra) 
        exmoSetMaxAlg(pi, &(en->algebra));
    else                 
        copyExpExmo(pi, &(en->algebra), algebra); 

    if (NULL == profile) 
        exmoSetMinAlg(pi, &(en->profile));
    else                  
        copyExpExmo(pi, &(en->profile), profile); 

    en->profile.ext &= en->algebra.ext;
    for (i=NALG; i--;)
        en->profile.dat[i] = MIN(en->profile.dat[i], en->algebra.dat[i]);

    return SUCCESS;
}

int enmSetSignature(enumerator *en, exmo *sig) {
    if (NULL == sig) 
        memset(&(en->signature), 0, sizeof(exmo));
    else                  
        copyExmo(&(en->signature), sig);
    enmDestroyEffList(en);
    enmUpdateSigInfo(en, &(en->signature), &(en->sigideg), &(en->sigedeg));
    /* sigideg & sigedeg are determined when Efflist is recreated */
    return SUCCESS;
}

int enmSetTridegree(enumerator *en, int ideg, int edeg, int hdeg) {
    enmDestroyEffList(en);
    en->ideg = ideg; en->hdeg = hdeg; en->edeg = edeg;
    return SUCCESS;
}

int enmSetGenlist(enumerator *en, int *gl, int num) {
    int i;
    enmDestroyGenList(en);
    en->genList = gl; 
    en->numgens = num;
    /* find max/min ideg/edeg */
    if (0 == num) {
        en->maxideg = en->minideg = en->maxedeg = en->minedeg = 0;
    } else {
        en->maxideg = en->maxedeg = -9999; en->minideg = en->minedeg = 9999;
    }
    en->maxgenid = 0;
    for (i=num;i--;gl+=4) {
        en->maxideg = MAX(en->maxideg, gl[1]);
        en->minideg = MIN(en->minideg, gl[1]);
        en->maxedeg = MAX(en->maxedeg, gl[2]);
        en->minedeg = MIN(en->minedeg, gl[2]);
        en->maxgenid = MAX(en->maxgenid, gl[0]);
    }
    if (ENLOG) printf("set genlist (%d gens)\n", num);
    return SUCCESS;
}

/**** SEQUENCE NUMBERS ******************************************************/

int enmCreateSeqtab(enumerator *en) {
    primeInfo *pi = en->pi;    
    int reddim, maxdim, i, j, k, n;
    exmo *alg = &(en->algebra), *pro = &(en->profile);

    if (NULL == pi) return FAILIMPOSSIBLE;
    enmDestroySeqtab(en);

    maxdim = 10 + (1 + en->maxrrideg) * en->pi->tpmo;
    
    reddim   = 1 + maxdim / pi->tpmo;
    en->tabmaxrideg = maxdim / pi->tpmo;
    en->tablen      = reddim+1;

    if (ENLOG) printf("creating dimtab & seqtab for prime %d upto ideg %d\n",
                      pi->prime,maxdim);

    /* allocate space */
    for (i=NALG+1;i--;)
        if (NULL == (en->dimtab[i] = mallox(sizeof(int) * (reddim+1)))) {
            enmDestroySeqtab(en);
            return FAILMEM;
        }

    for (i=NALG+1;i--;)
        if (NULL == (en->seqtab[i] = mallox(sizeof(int) * (reddim+1)))) {
            enmDestroySeqtab(en);
            return FAILMEM;
        }

#define BIGVAL (65536.0 * 16000.0)

    /* compute effective degrees */
    for (i=NALG;i--;) {
        double aux = pi->reddegs[i];
        aux *= pro->dat[i];
        if ((i>pi->maxpowerXintI) || (aux > BIGVAL) || (-aux > BIGVAL)) {
            /* we choose a very big fantasy value */
            en->effdeg[i] = 0xeffffff;
        } else { 
            en->effdeg[i] = pi->reddegs[i];
            en->effdeg[i] *= pro->dat[i]; 
        }
    }
    
    /* create dimension table: */

    /* dimtab[0] is the dimension of k[xi_1] (restricted to A//B) */
    for (j=0;j<reddim;j++)
        if ((j < alg->dat[0]) && ((j % pro->dat[0]) == 0))
            en->dimtab[0][j] = 1;
        else
            en->dimtab[0][j] = 0;
    
    /* now dimtab[i] for k[xi_1,...,xi_{i+1}] */
    for (i=1;i<NALG;i++)
        for (j=0;j<reddim;j++) {
            int sum=0, d = pi->reddegs[i];
            for (k=j/d;k>=0;k--)
                if ((k<alg->dat[i]) && ((k % pro->dat[i]) == 0))
                    sum += en->dimtab[i-1][j-k*d];
            en->dimtab[i][j] = sum;
        }
    
    /* next is seqtab */

    en->seqtab[0][0] = 0; /* this is all we need from seqtab[0][...] */

    /* seqtab[k+1][n] = dimtab[k][n-d] + dimtab[k][n-2d] + ... + dimtab[k][n%d]
     * where d = effective degree */
    for (k=1;k<NALG;k++)
        for (n=0;n<reddim;n++) {
            int sum=0, d = en->effdeg[k];
            for (i=n-d;i>=0;i-=d)
                sum += en->dimtab[k-1][i];
            en->seqtab[k][n] = sum;
        }

    if (ENLOG) {
        printf("dimtab & seqtab info (prime %d, en %p):\n", en->pi->prime, en);
        printf("en->effdeg ="); 
        for (i=0;i<NALG;i++) printf(" %d",en->effdeg[i]);
        printf("\nbeginning of dimtab:\n");
        for (i=0;i<NALG;i++) {
            printf("  dimtab[%d] (at %p) = ",i,en->dimtab[i]);
            for (n=0;n<10;n++) printf(" %d",en->dimtab[i][n]); 
            printf("...\n");
        }
        printf("\nbeginning of seqtab: (seqtab[0][0] = %d)\n",en->seqtab[0][0]);
        for (i=1;i<NALG;i++) {
            printf("  seqtab[%d] (at %p) = ",i,en->seqtab[i]);
            for (n=0;n<10;n++) printf(" %d",en->seqtab[i][n]); 
            printf("...\n");
        }
    }

    return SUCCESS;
}

int algDimension(enumerator *en, int rdim) {
    return en->dimtab[NALG-1][rdim];
}

int enmCreateSeqoff(enumerator *en) {
    int cnt, i;

    if (NULL == en->efflist) 
        if (SUCCESS != (i = enmRecreateEfflist(en)))
            return i;

    if (en->maxrrideg > en->tabmaxrideg) 
        enmDestroySeqtab(en);

    if (NULL == en->seqtab[0])  
        if (SUCCESS != enmCreateSeqtab(en)) 
            return FAILMEM;
    
    if (NULL == (en->seqoff = (int *) mallox(en->efflen * sizeof(int))))
        return FAILMEM;

    for (cnt=i=0; i<en->efflen; i++) {
        en->seqoff[i] = cnt;
        if (ENLOG) printf("seqoff (gen %3d ext %3d) = %d\n",
                          en->efflist[i].id, en->efflist[i].ext, cnt);
        cnt += algDimension(en, en->efflist[i].rrideg);
    }

    en->totaldim = cnt;

    return SUCCESS;
}

/* a seqno version that just uses the algebra pair, not the module */
int algSeqnoWithRDegree(enumerator *en, exmo *ex, int deg) {
    int res=0, k, startk;
    startk = en->pi->maxpowerXintI;
    if (ENLOG) printf("algSeqnoWithRDegree deg = %d, startk = %d\n", deg, startk);
    for (k=startk; k--;) {
        int prd, maxdeg, actdeg, exo;
        prd = MIN(en->profile.dat[k], en->algebra.dat[k]); 
        maxdeg = (en->algebra.dat[k] - prd) * en->pi->reddegs[k];
        exo = en->ispos ? ex->dat[k] : (-1 - ex->dat[k]); 
        exo /= en->profile.dat[k]; exo *= en->profile.dat[k];
        actdeg = exo * en->pi->reddegs[k];
        if (deg < maxdeg) maxdeg = deg;
        if ((deg < actdeg) 
            || ((deg - actdeg) >= en->tablen) 
            || ((deg - maxdeg) >= en->tablen)) {
            if (ENLOG) printf("in algSeqno...: deg=%d, actdeg=%d\n", deg, actdeg);
            return -1;
        }
        res += en->seqtab[k][deg - actdeg] - en->seqtab[k][deg - maxdeg];
        deg -= actdeg;
    }
    return res;
}

int SeqnoFromEnum(enumerator *en, exmo *ex) {
    effgen aux, *res; 
    int cnt, cnt2;

    if (NULL == en->seqoff) 
        if (SUCCESS != enmCreateSeqoff(en))
            return -666;

    aux.id  = ex->gen;
    aux.ext = en->ispos ? ex->ext : (-1 - ex->ext); 
    aux.ext ^= (aux.ext & en->profile.ext);
    res = (effgen *) bsearch(&aux, en->efflist, en->efflen, 
                             sizeof(effgen), compareEffgen);
    if (NULL == res) return -1;

    cnt = en->seqoff[res - en->efflist];
    cnt2 = algSeqnoWithRDegree(en, ex, res->rrideg);
    if (cnt2 < 0) return -1;
    return cnt + cnt2;
}

int DimensionFromEnum(enumerator *en) {
    if (NULL == en->seqoff) 
        if (SUCCESS != enmCreateSeqoff(en))
            return -666;
    return en->totaldim;
}

/**** ENUMERATION ************************************************************/

int firstRedmonAlg(enumerator *en, int deg);

int nextExmoAux(enumerator *en) {
    int msk, edeg;
    exmo *ex = &(en->varex);
    do {
        if (0 == (msk=ex->ext)) return 0;
        while (0 != ((--msk) & (en->profile.ext))) ;
        edeg = extdeg(en->pi, msk); ex->ext = msk;
        en->remdeg = en->totdeg - edeg;
        if (firstRedmonAlg(en, en->remdeg)) return 1;
    } while (1);
}

int firstExmon(enumerator *en, int deg) {
    exmo *ex = &(en->varex);
    en->remdeg = en->totdeg = deg;
    ex->ext = getMaxExterior(en->pi, &(en->algebra), &(en->profile), en->remdeg);
    en->remdeg -=  extdeg(en->pi, ex->ext);
    en->extdeg = deg - en->remdeg;
    if (firstRedmonAlg(en, en->remdeg)) return 1;
    return nextExmoAux(en);
}

int nextRedmonAlg(enumerator *en);

int nextExmon(enumerator *en) {
    if (nextRedmonAlg(en)) return 1;
    return nextExmoAux(en);
}

int firstRedmonAlg(enumerator *en, int rdeg) {
    exmo *ex = &(en->varex);
    int i;
    for (i=NALG;i--;) {
        int nval = rdeg / en->pi->reddegs[i];
        /* restrict redpows to the algebra */
        if (nval > en->algebra.dat[i]-1) 
            nval = en->algebra.dat[i]-1;
        /* remove part forbidden by profile */
        nval /= en->profile.dat[i]; 
        nval *= en->profile.dat[i];
        ex->dat[i] = nval;
        en->theex.dat[i] = nval + en->signature.dat[i];
        if (!en->ispos) en->theex.dat[i] = -1 - en->theex.dat[i];
        rdeg -= nval * en->pi->reddegs[i];
    }
    en->errdeg = rdeg;
    if (rdeg) return nextRedmonAlg(en);
    return 1;
}

int nextRedmonAlg(enumerator *en) {
    exmo *ex = &(en->varex);
    int rem, i;
    do {
        int nval;
        rem = en->errdeg + ex->dat[0];
        for (i=1;0==ex->dat[i];)
            if (++i >= NALG) return 0;
        nval = ex->dat[i] - 1;
        nval /= en->profile.dat[i]; 
        nval *= en->profile.dat[i];
        rem += (ex->dat[i] - nval) * en->pi->reddegs[i];
        ex->dat[i] = nval;
        en->theex.dat[i] = nval + en->signature.dat[i];
        if (!en->ispos) en->theex.dat[i] = -1 - en->theex.dat[i];
        for (;i--;) {
            nval = rem / en->pi->reddegs[i];
            /* restrict redpows to the algebra */
            if (nval>en->algebra.dat[i]-1) 
                nval=en->algebra.dat[i]-1;
            /* remove part forbidden by profile */
            nval /= en->profile.dat[i]; 
            nval *= en->profile.dat[i];
            ex->dat[i] = nval;
            en->theex.dat[i] = nval + en->signature.dat[i];
            if (!en->ispos) en->theex.dat[i] = -1 - en->theex.dat[i];
             rem -= nval * en->pi->reddegs[i];
        }
        en->errdeg = rem;
    } while (en->errdeg);
    return 1;
}

int nextRedmonAux(enumerator *en) {
    while (en->gencnt < en->efflen) {
        if (firstRedmonAlg(en, en->efflist[en->gencnt].rrideg)) {
            en->theex.gen = en->efflist[en->gencnt].id;
            en->theex.ext = en->efflist[en->gencnt].ext | en->signature.ext;
            if (!en->ispos) 
                en->theex.ext = -1 - en->theex.ext;
            return 1;
        }
        ++(en->gencnt);
    }
    return 0;
}

int firstRedmon(enumerator *en) {
    if (NULL == en->efflist) 
        enmRecreateEfflist(en);
    en->gencnt = 0;
    en->theex.coeff = 1;
    return nextRedmonAux(en);
}

int nextRedmon(enumerator *en) {
    if (nextRedmonAlg(en)) return 1;
    ++(en->gencnt);
    return nextRedmonAux(en);
}

/**** SIGNATURE ENUMERATION **************************************************/

int nextSignature(enumerator *en, exmo *sig, int *sideg, int *sedeg) {
    int _sideg, _sedeg, i, maxi, maxe, remi, tpmo, newext;
    if ((NULL == sideg) || (NULL == sedeg)) {
        sideg = &_sideg; sedeg = &_sedeg;
        enmUpdateSigInfo(en, sig, sideg, sedeg);
    }
    maxi = en->ispos ? (en->ideg - en->minideg) : (en->maxideg - en->ideg);
    maxe = en->ispos ? (en->edeg - en->minedeg) : (en->maxedeg - en->edeg);
    tpmo = en->pi->tpmo;
    remi = maxi - *sideg; /* remaining degree */
    for (i=0;i<NALG;i++) {
        int coldeg = tpmo * en->pi->reddegs[i];
        int nval = sig->dat[i] + 1;
        int aux;
        /* see if we can advance this entry */
        if ((coldeg <= remi) 
            && (nval < en->algebra.dat[i]) 
            && (nval < en->profile.dat[i])) {
            sig->dat[i]++; *sideg += coldeg; 
            return 1;
        }   
        /* reset entry */
        aux = coldeg * (--nval);
        *sideg -= aux; remi += aux; sig->dat[i] = 0;
    }
    /* reduced part has been reset; try to advance exterior component */
    newext = sig->ext;
    do {
        *sideg = extdeg(en->pi, ++newext);
        if (*sideg > maxi) break;
        if (newext == (newext & en->algebra.ext & en->profile.ext)) {
            sig->ext = newext;
            *sedeg = BITCOUNT(newext);
            return 1;
        }
    } while (1);
    return 0;
}

int enmIncrementSig(enumerator *en) {
    int res = nextSignature(en, &(en->signature), &(en->sigideg), &(en->sigedeg));
    enmDestroyEffList(en);
    return res;
}




polyType enumPolyType = {
#if 0
    .getInfo    = &epoGetInfo,
    .free       = &epoFree,
    .createCopy = &epoCreateCopy,
    .getNumsum  = &epoGetNumsum,
    .getExmoPtr = &epoGetExmoPtr
#endif
};
