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

#include <profile.h>

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

void clearProcore(procore *core, int rval) {
    int i;
    core->edat = 0;
    for (i=NPRO;i--;) core->rdat[i]=rval;
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
    for (i=pi->N; i && (pi->extdegs[i] > maxdim);) 
        i--;
    val = i;
    for (i=0; i<NPRO; i++) {
        pro->core.rdat[i] = pi->primpows[val]; 
        if (val) val--;
    }
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

enumEnv *createEnumEnv(primeInfo *pi, profile *alg, profile *pro, int maxdim) {
    enumEnv *res = cmalloc(sizeof(enumEnv)); 
    if (NULL==res) return NULL;
    /* we keep references to our parameters (not copies) */
    res->pi = pi;
    res->pro=pro;
    res->alg=alg;
    /* private initialization follows */


    return res;
}

void disposeEnumEnv(enumEnv *env) {
    /* don't destroy given parameters, just the derived private data */


}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

/* largest exterior that is contained in *alg and allowed by *pro */
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
        rem = ex->errdeg + ex->core.rdat[0];
        for (i=1;0==ex->core.rdat[i];)
            if (++i >= NPRO) return 0;
        rem += env->pi->reddegs[i]; ex->core.rdat[i]--;
        for (;i--;) {
            int nval = rem / env->pi->reddegs[i];
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

