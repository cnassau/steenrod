/*
 * All about profiles, subalgebras, and enumeration
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

#include "profile.h"

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

void clearProcore(procore *core, int rval) {
    int i;
    core->edat = 0;
    for (i=NPRO;i--;) core->rdat[i]=rval;
}

int reddegProcore(procore *core, primeInfo *pi) {
    int res=0, i;
    for (i=NPRO;i--;) res += pi->reddegs[i] * core->rdat[i];
    return res;
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

void makeZeroProfile(profile *pro) {
    int i;
    pro->core.edat = 0;
    for (i=NPRO;i--;) pro->core.rdat[i]=1;
}


void makeFullProfile(profile *pro, primeInfo *pi, int maxdim) {
    int i, val;
    pro->core.edat = ~0;
    for (i=NALG; i && (pi->extdegs[i] > maxdim);) 
        i--;
    val = i;
    for (i=0; i<NPRO; i++) {
        pro->core.rdat[i] = pi->primpows[val]; 
        if (val) val--;
    }
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

enumEnv *createEnumEnv(primeInfo *pi, profile *alg, profile *pro) {
    enumEnv *res = mallox(sizeof(enumEnv)); 
    if (NULL==res) return NULL;
    /* we keep references to our parameters (not copies) */
    res->pi = pi;
    res->pro=pro;
    res->alg=alg;
    return res;
}

void disposeEnumEnv(enumEnv *env) {
    /* don't destroy given parameters, just the derived private data */
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

int getMaxExterior(enumEnv *env, int *deg) {
    int res=0, wrk=1<<NPRO, i=NPRO;
    while (wrk>>=1,i--) 
        if (0 == (wrk&(env->pro->core.edat)))
            if (wrk == (wrk&(env->alg->core.edat)))
                if (env->pi->extdegs[i] <= *deg) {
                    res |= wrk; *deg -= env->pi->extdegs[i];
                }
    return res;
}

int nextExmonAux(exmon *ex, enumEnv *env) {
    int msk,edeg;
    do {
        if (0 == (msk=ex->core.edat)) return 0;
        while (0 != ((--msk)&(env->pro->core.edat))) ;
        edeg=extdeg(env->pi, msk); ex->core.edat = msk;
        ex->remdeg = ex->totdeg - edeg;
        if (firstRedmon(ex, env, ex->remdeg)) return 1;
    } while (1);
}

int firstExmon(exmon *ex, enumEnv *env, int deg) {
    ex->remdeg = ex->totdeg = deg;
    ex->core.edat = getMaxExterior(env, &ex->remdeg);
    ex->extdeg = deg - ex->remdeg;
    if (firstRedmon(ex, env, ex->remdeg)) return 1;
    return nextExmonAux(ex, env);
}

int nextExmon(exmon *ex, enumEnv *env) {
    if (nextRedmon(ex, env)) return 1;
    return nextExmonAux(ex, env);
}

int firstRedmon(exmon *ex, enumEnv *env, int deg) {
    int i;
    if (0 != (deg % (env->pi->tpmo))) return 0;
    deg /= env->pi->tpmo;
    for (i=NPRO;i--;) {
        int nval = deg / env->pi->reddegs[i];
        /* restrict redpows to env->alg */
        if (nval>env->alg->core.rdat[i]-1) nval=env->alg->core.rdat[i]-1;
        /* remove part forbidden by env->pro */
        nval /= env->pro->core.rdat[i]; nval *= env->pro->core.rdat[i];
        ex->core.rdat[i] = nval;
        deg -= nval * env->pi->reddegs[i];
    }
    ex->errdeg = deg;
    if (deg) return nextRedmon(ex, env);
    return 1;
}

int nextRedmon(exmon *ex, enumEnv *env) {
    int rem, i;
    do {
        int nval;
        rem = ex->errdeg + ex->core.rdat[0];
        for (i=1;0==ex->core.rdat[i];)
            if (++i >= NPRO) return 0;
        nval = ex->core.rdat[i] - 1;
        nval /= env->pro->core.rdat[i]; nval *= env->pro->core.rdat[i];       
        rem += (ex->core.rdat[i]-nval) * env->pi->reddegs[i]; 
        ex->core.rdat[i] = nval;
        for (;i--;) {
            nval = rem / env->pi->reddegs[i];
            /* restrict redpows to env->alg */
            if (nval>env->alg->core.rdat[i]-1) nval=env->alg->core.rdat[i]-1;
            /* remove part forbidden by env->pro */
            nval /= env->pro->core.rdat[i]; nval *= env->pro->core.rdat[i];       

            ex->core.rdat[i] = nval;
            rem -= nval * env->pi->reddegs[i];
        }
        ex->errdeg = rem;
    } while (ex->errdeg); 
    return 1;
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

/* enumeration of signatures for a given profile */

void firstSignature(enumEnv *env, exmon *exm, int maxdeg) {
    clearProcore(&(exm->core), 0);
    exm->totdeg = 0;
    exm->remdeg = maxdeg;
}

int nextSignExt(enumEnv *env, exmon *exm) {
    int e = exm->core.edat, d; 
    e = (~e) & env->pro->core.edat;
    if (!e) return 0;
    e = (~(e-1)) & env->pro->core.edat;
    d = extdeg(env->pi, e) - extdeg(env->pi, exm->core.edat);
    exm->core.edat = e; 
    if (exm->remdeg>=0)
        if (exm->remdeg < d)
            return 0;
    exm->remdeg -= d;
    exm->totdeg += d;
    return 1;
}

int nextSignRed(enumEnv *env, exmon *exm) {
    int i, tpmo = env->pi->tpmo;
    procore *eco = &(exm->core), *pco = &(env->pro->core);
    for (i=0;i<NPRO;i++) {
        int aux = tpmo * env->pi->reddegs[i];
        if ((eco->rdat[i]+1) < pco->rdat[i])
            if (exm->remdeg<0 || (exm->remdeg >= (aux))) {
                eco->rdat[i]++; 
                exm->remdeg -= aux;
                exm->totdeg += aux;
                return 1;
            } 
        exm->remdeg += eco->rdat[i] * aux; 
        exm->totdeg -= eco->rdat[i] * aux; 
        eco->rdat[i] = 0;
    }
    /* could not increase reduced part, go to next exterior */
    return nextSignExt(env, exm);
}

int nextSignature(enumEnv *env, exmon *exm) {
    return nextSignRed(env, exm);
}

void setSignature(exmon *dst, enumEnv *env, exmon *src, exmon *sig) {
    int i;
    dst->core.edat = (env->pro->core.edat & sig->core.edat) 
        || (~(env->pro->core.edat) & src->core.edat);
    for (i=NPRO;i--;) {
        int pd = env->pro->core.rdat[i];
        dst->core.rdat[i] = (src->core.rdat[i] / pd) * pd + sig->core.rdat[i];
    }
    dst->id = src->id;
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

seqnoInfo *createSeqno(enumEnv *env, int maxdim) {
    int reddim, i, j, k, n;
    primeInfo  *pi = env->pi;
    procore   *alg = &(env->alg->core);
    procore   *pro = &(env->pro->core);
    seqnoInfo *res = calloc(1,sizeof(seqnoInfo));

    if (NULL == res) return NULL;

    res->env = env;
    res->pi  = pi;
    reddim   = 1 + maxdim / pi->tpmo;

    for (i=NPRO+1;i--;)
        if (NULL == (res->dimtab[i] = mallox(sizeof(int) * (reddim+1)))) {
            destroySeqno(res);
            return NULL;
        }

    for (i=NPRO+1;i--;)
        if (NULL == (res->seqtab[i] = mallox(sizeof(int) * (reddim+1)))) {
            destroySeqno(res);
            return NULL;
        }
    
    for (i=NPRO;i--;) {
        double aux = pi->reddegs[i]; aux *= pro->rdat[i];
        if (aux > (65536.0 * 32000.0)) {
            res->effdeg[i] = 0xefffffff;
        } else { res->effdeg[i] = pi->reddegs[i] * pro->rdat[i]; }
    }

    /* create dimension table: */
 
    /* dimtab[0] is the dimension of k[xi_1] (restricted to A//B) */ 
    for (j=0;j<reddim;j++) 
        if ((j<alg->rdat[0]) && ((j % pro->rdat[0]) == 0))
            res->dimtab[0][j] = 1;
        else 
            res->dimtab[0][j] = 0;
    
    /* now dimtab[i] for k[xi_1,...,xi_{i+1}] */
    for (i=1;i<NPRO;i++) 
        for (j=0;j<reddim;j++) {
            int sum=0, d = pi->reddegs[i];
            for (k=j/d;k>=0;k--)
                if ((k<alg->rdat[i]) && ((k % pro->rdat[i] == 0)))
                    sum += res->dimtab[i-1][j-k*d];
            res->dimtab[i][j] = sum;
        }

    /* next is seqtab */
    
    res->seqtab[0][0] = 0; /* this is all we need from seqtab[0][...] */

    /* seqtab[k+1][n] = dimtab[k][n-d] + dimtab[k][n-2d] + ... + dimtab[k][n%d] 
     * where d = effective degree */
    for (k=1;k<NPRO;k++) 
        for (n=0;n<reddim;n++) {
            int sum=0, d = res->effdeg[k];
            for (i=n-d;i>=0;i-=d)
                sum += res->dimtab[k-1][i];
            res->seqtab[k][n] = sum;
        }
    return res;
}

void destroySeqno(seqnoInfo *s) {
    int i;
    for (i=NPRO;i>=0;i--) {
        if (NULL != (s->dimtab[i])) freex(s->dimtab[i]);
        if (NULL != (s->seqtab[i])) freex(s->seqtab[i]);
    }
    freex(s);
}

int SqnInfGetDim(seqnoInfo *sqn, int dim) {
    if (0 != (dim % sqn->pi->tpmo)) return 0;
#if 0
    {
        int i,j;
        for (i=0;i<6;i++) {
            for (j=0;j<25;j++)
                printf(" %2d",sqn->dimtab[i][j]);
            printf("\n");
        }
    }
#endif
    return sqn->dimtab[NPRO-1][dim / sqn->pi->tpmo];
}

int SqnInfGetSeqno(seqnoInfo *sqn, exmon *ex) {
    return 0;
}

int SqnInfGetSeqnoWithDegree(seqnoInfo *sqn, exmon *ex, int deg) {
    int res=0, k;
    deg /= sqn->pi->tpmo;
    for (k=NPRO-1;k--;) {
        int prd = sqn->env->pro->core.rdat[k];
        int maxdeg = (sqn->env->alg->core.rdat[k] - prd) * sqn->pi->reddegs[k];
        int actdeg = (ex->core.rdat[k] / 1) * 1 * sqn->pi->reddegs[k];
        if (maxdeg>deg) maxdeg = deg; 
        res += sqn->seqtab[k][deg - actdeg] - sqn->seqtab[k][deg - maxdeg];
#if 0
        printf("+ %d - %d\n",sqn->seqtab[k][deg - actdeg],sqn->seqtab[k][deg-maxdeg]);  
#endif
        deg -= actdeg;
    }
    return res;
}
