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
    memcpy(en, src, sizeof(en));

    en->pi = src->pi;
    copyExmo(&(en->algebra), &(src->algebra));
    copyExmo(&(en->profile), &(src->profile));

    /* ... but clear pointers */
    en->genList = NULL;
    for (i=NALG+1;i--;) 
        en->dimtab[i] = en->seqtab[i] = NULL;

    en->efflist = NULL; 
    en->seqoff = NULL;

    /* now make private copies of the arrays */
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
    for (i=0;i<NALG+1;i++) {
        FREEPTR(en->dimtab[i]);
        FREEPTR(en->seqtab[i]);
    }
    enmDestroySeqOff(en);
}

void enmDestroyEffList(enumerator *en) {
    FREEPTR(en->efflist);
    en->efflen = 0;
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

int compareEffgen(const void *a, const void *b) {
    const effgen *aa = (const effgen *) a;
    const effgen *bb = (const effgen *) b;
    int res;
    if ((res = (aa->id - bb->id))) return res;
    return aa->ext - bb->ext;
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
        int aux = ex->dat[i];
        aux /= prof->dat[i]; aux *= prof->dat[i];
        ex->dat[i] -= aux;
    }
}

void enmUpdateSigInfo(enumerator *en) {
    /* clip signature to profile and determine degrees */
    clipExmo(&(en->signature), &(en->profile));
    en->sigedeg = BITCOUNT(en->signature.ext);
    en->sigideg = exmoIdeg(en->pi, &(en->signature));
}

/* create list of effective generators for the given tridegree */
int enmRecreateEfflist(enumerator *en) {
    int i, *glp, ext, tpmo, cnt, tideg, tedeg, thdeg;
    if (NULL == en->pi) return FAILIMPOSSIBLE;
    /* if present: destroy old values */
    enmDestroyEffList(en);
    en->maxrrideg = 0;
    /* find real tridegree coords */
    enmUpdateSigInfo(en);
    thdeg = en->hdeg;
    tedeg = en->edeg - en->sigedeg;
    tideg = en->ideg - en->sigideg;
    /* go through all gens in the genlist and look for appropriate exteriors */
    for (cnt=i=0,tpmo=en->pi->tpmo,glp=en->genList; i<en->numgens; i++,glp+=4) {
        int id = glp[0], ideg = glp[1], edeg = glp[2], hdeg = glp[3];
        if (ENLOG) printf("looking at generator with "
                          "id=%d ideg=%d edeg=%d hdeg=%d\n", id, ideg, edeg, hdeg);
        if ((edeg <= tedeg) && (ideg <= tideg) && (hdeg == en->hdeg)) {
            int rideg = tideg - ideg, redeg = tedeg - edeg;
            ext = getMaxExterior(en->pi, &(en->algebra), &(en->profile), rideg);
            if (1 && ENLOG) printf("maxExterior=%d (rideg=%d)\n", ext, rideg);
            for (;ext>=0;ext--) 
                if (BITCOUNT(ext) == redeg) {
                    int extideg = extdeg(en->pi, ext);
                    if (extideg < rideg) {
                        int diffideg = rideg - extideg;
                        if (0 == (diffideg % tpmo)) {
                            effgen *gen;
                            /* we have a winner! */
                            if ((cnt == en->efflen) &&
                                (SUCCESS != enmReallocEfflist(en, en->efflen+500))) {
                                enmDestroyEffList(en);
                                return FAILMEM;
                            }
                            gen = &(en->efflist[cnt++]);
                            gen->id     = id;
                            gen->ext    = ext;
                            gen->rrideg = diffideg / tpmo;
                            if (gen->rrideg > en->maxrrideg) 
                                en->maxrrideg = gen->rrideg;
                            if (ENLOG) printf("  effgen id=%d ext=%d rrideg=%d\n",
                                              id, ext, gen->rrideg);
                            en->efflen++;
                        }
                    }
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

int enmSetBasics(enumerator *en, primeInfo *pi, exmo *algebra, exmo *profile) {
    enmDestroyEffList(en);
    enmDestroySeqtab(en);
    en->pi = pi;

    if (NULL == algebra) 
        exmoSetMaxAlg(pi, &(en->algebra));
    else                 
        copyExpExmo(pi, &(en->algebra), algebra); 

    if (NULL == profile) 
        exmoSetMinAlg(pi, &(en->profile));
    else                  
        copyExpExmo(pi, &(en->profile), profile); 

    return SUCCESS;
}

int enmSetSignature(enumerator *en, exmo *sig) {
    enmDestroyEffList(en);
    if (NULL == sig) 
        memset(&(en->signature), 0, sizeof(exmo));
    else                  
        copyExmo(&(en->signature), sig);
    /* sigideg & sigedeg are determined when efflist is recreated */
    return SUCCESS;
}

int enmSetTridegree(enumerator *en, int ideg, int edeg, int hdeg) {
    enmDestroyEffList(en);
    en->ideg = ideg; en->hdeg = hdeg; en->edeg = edeg;
    return SUCCESS;
}

int enmSetGenlist(enumerator *en, int *gl, int num) {
    enmDestroyGenList(en);
    en->genList = gl; 
    en->numgens = num;
    if (ENLOG) printf("set genlist (%d gens)\n", num);
    return SUCCESS;
}

/**** SEQUENCE NUMBERS ******************************************************/

int enmCreateSeqtab(enumerator *en, int maxdim) {
    primeInfo *pi = en->pi;    
    int reddim, i, j, k, n;
    exmo *alg = &(en->algebra), *pro = &(en->profile);

    if (NULL == pi) return FAILIMPOSSIBLE;
    enmDestroySeqtab(en);
    
    reddim   = 1 + maxdim / pi->tpmo;
    en->tabmaxrideg = maxdim / pi->tpmo;
    en->tablen      = reddim+1;

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

    /* compute effective degrees */
    for (i=NALG;i--;) {
        double aux = pi->reddegs[i]; 
        aux *= pro->dat[i];
        if (aux > (65536.0 * 32000.0)) {
            /* we choose a very big fantasy value */
            en->effdeg[i] = 0xefffffff;
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

    return SUCCESS;
}

/* a seqno version that just uses the algebra pair, not the module */
int algSeqnoWithRDegree(enumerator *en, exmo *ex, int deg) {
    int res=0, k;
    for (k=NALG-1;k--;) {
        int prd, maxdeg, actdeg;
        prd = en->profile.dat[k];
        maxdeg = (en->algebra.dat[k] - prd) * en->pi->reddegs[k];
        actdeg = ex->dat[k] * en->pi->reddegs[k];
        if (maxdeg > deg) maxdeg = deg;
        res += en->seqtab[k][deg - actdeg] - en->seqtab[k][deg - maxdeg];
        deg -= actdeg;
    }
    return res;
}

int algDimension(enumerator *en, int rdim) {
    return en->dimtab[NALG-1][rdim];
}

int SeqnoFromEnum(enumerator *en, exmo *ex) {
    effgen aux, *res; 
    int cnt;
    aux.id  = ex->gen;
    aux.ext = ex->ext;
    res = (effgen *) bsearch(&aux, en->efflist, en->efflen, 
                             sizeof(effgen), compareEffgen);
    if (NULL == res) return -1;
    cnt = en->seqoff[res - en->efflist];
    cnt += algSeqnoWithRDegree(en, ex, res->rrideg);
    return cnt;
}

int enmCreateSeqoff(enumerator *en) {
    int cnt, i;

    if (NULL == en->efflist) return FAILIMPOSSIBLE;

    enmDestroySeqOff(en); 
    
    if (NULL == (en->seqoff = (int *) mallox(en->efflen * sizeof(int))))
        return FAILMEM;

    for (cnt=i=0; i<en->efflen; i++) {
        en->seqoff[i] = cnt;
        cnt += algDimension(en, en->efflist[i].rrideg);
    }

    return SUCCESS;
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
        for (;i--;) {
            nval = rem / en->pi->reddegs[i];
            /* restrict redpows to the algebra */
            if (nval>en->algebra.dat[i]-1) 
                nval=en->algebra.dat[i]-1;
            /* remove part forbidden by profile */
            nval /= en->profile.dat[i]; 
            nval *= en->profile.dat[i];
            ex->dat[i] = nval;
            rem -= nval * en->pi->reddegs[i];
        }
        en->errdeg = rem;
    } while (en->errdeg);
    return 1;
}

int nextRedmonAux(enumerator *en) {
    while (en->gencnt < en->efflen) {
        if (firstRedmonAlg(en, en->efflist[en->gencnt].rrideg)) {
            en->varex.gen = en->efflist[en->gencnt].id;
            en->varex.ext = en->efflist[en->gencnt].ext;
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
    return nextRedmonAux(en);
}

int nextRedmon(enumerator *en) {
    if (nextRedmonAlg(en)) return 1;
    ++(en->gencnt);
    return nextRedmonAux(en);
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
