/*
 * Representation of a map from one freex module to another
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

#include <string.h>
#include <tcl.h>
#include "maps.h"

void mapsumInit(mapsum *res) {
    res->num = res->alloc = 0;
    res->len = 0; 
    res->cdat = NULL; res->xdat = NULL;
}

void mapsumDestroy(mapsum *mp) {
    if (0 != mp->alloc) {
        freex(mp->cdat); 
        freex(mp->xdat);
    }
}

int mapsumSetPad(mapsum *mps, int pad) {
    if (0 == mps->num) { 
        mps->pad = pad;
        return SUCCESS;
    }
    if (mps->pad != pad) return FAILIMPOSSIBLE;
    return SUCCESS;
}

int mapsumSetLen(mapsum *mp, int len) {
    xint *nxd, *src, *dst; 
    int i, j, min, max;
    if (mp->len == len) return SUCCESS;
    if (0 == mp->num) {
        if (0!=mp->alloc) { freex(mp->xdat); freex(mp->cdat); }
        mp->alloc = 0;
        mp->len = len;
        return SUCCESS;
    }
    /* need to reformat existing data... */
    if (len < mp->len) {
        /* check if we can safely cut off extra values */
        for (i=mp->num,src=mp->xdat;i--;src+=mp->len)
            for (j=len;j<mp->len;j++) 
                if (mp->pad != src[j]) 
                    return FAILIMPOSSIBLE;
    }
    min = len; max = mp->len;
    if (min>max) { min = mp->len; max = len; }  
    nxd = mallox(len * mp->alloc * sizeof(xint));
    if (NULL == nxd) return FAILMEM;
    for (dst=nxd,src=mp->xdat,i=mp->num;i--;dst+=len,src+=mp->len) {
      for (j=0;j<min;j++) dst[j] = src[j];
      for (j=min;j<max;j++) dst[j] = mp->pad;
    }
    mp->len = len;
    freex(mp->xdat);
    mp->xdat = nxd;
    return SUCCESS;
}

int mapsumRealloc(mapsum *mp, int nalloc) {
    cint *ncd; xint *nxd;
    if (nalloc < mp->num) nalloc = mp->num;
    if (NULL == (ncd = reallox(mp->cdat, nalloc * sizeof(cint)))) {
        return FAILMEM;
    }
    mp->cdat = ncd;
    if (NULL == (nxd = reallox(mp->xdat, mp->len * nalloc * sizeof(xint)))) {
        return FAILMEM;
    }    
    mp->xdat = nxd;
    mp->alloc = nalloc;
    return SUCCESS;
}

int mapsumCompFunc(const void *aa, const void *bb) {
    const mapsum *a = (const mapsum *) aa;
    const mapsum *b = (const mapsum *) bb;
    int res;
    if ((res = (a->gen - b->gen))) return res;
    if ((res = (a->edat - b->edat))) return res;
    return 0;
}

/* We can assume that each Tcl_obj in the array can be converted to int. */
int mapsumAppendFromList(mapsum *mps, int len, int obc, Tcl_Obj **obv) {
    int num, pad, i, ivar;
    Tcl_Interp *ip = NULL; 
    cint *cptr; xint *xptr; 
    if (0 != (obc % (len + 1))) return FAILIMPOSSIBLE;
    num = obc / (len + 1);
    if (len > mps->len) 
        if (SUCCESS != mapsumSetLen(mps, len))
            return FAILIMPOSSIBLE;
    if (mps->num + num > mps->alloc) 
        if (SUCCESS != mapsumRealloc(mps, mps->num + num))
            return FAILMEM;
    cptr = mps->cdat + mps->num; 
    xptr = mps->xdat + mps->num * mps->len; 
    pad = mps->pad;
    mps->num += num;
    while (num--) {
        Tcl_GetIntFromObj(ip, *(obv++), &ivar);
        *cptr++ = ivar;
        for (i=0;i<len;i++) {
            Tcl_GetIntFromObj(ip, *(obv++), &ivar);
            *xptr++ = ivar;
        }
        for (;i<mps->len;i++) 
            *xptr++ = pad;
    }
    return SUCCESS;
}



void mapgenInit(mapgen *res) {
    res->num = res->alloc = 0; 
    res->dat = NULL;
}

int mapgenRealloc(mapgen *mim, int nalloc) {
    mapsum *ndat; 
    if (nalloc < mim->num) nalloc = mim->num;
    ndat = reallox(mim->dat, nalloc * sizeof(mapsum)); 
    if (NULL == ndat) return FAILMEM;
    mim->alloc = nalloc;
    mim->dat = ndat;
    return SUCCESS;
}
  
void mapgenDestroy(mapgen *mim) {
    int i;
    for (i=mim->num;i--;)
        mapsumDestroy(&(mim->dat[i]));
    if (mim->alloc)
        freex(mim->dat);
}

mapsum *mapgenCreateSum(mapgen *mpg, int gen, int edat) {
    mapsum *res;
    if (mpg->num == mpg->alloc) 
        if (SUCCESS != mapgenRealloc(mpg, mpg->alloc + 20))
            return NULL;
    res = &(mpg->dat[mpg->num++]);
    mapsumInit(res);
    res->gen = gen; res->edat = edat;
    return res;
}

mapsum *mapgenFindSum(mapgen *mpi, int gen, int edat) {
    mapsum aux, *res;
    aux.gen = gen;
    aux.edat = edat;
    res = bsearch(&aux, mpi->dat, mpi->num, sizeof(mapsum), mapsumCompFunc);
    return res;
}

map *mapCreate(void) {
    map *res = mallox(sizeof(map));
    if (NULL == res) return NULL;
    memset(res, 0, sizeof(map));
    return res;
}

int mapRealloc(map *mp, int nalloc) {
    mapgen *mpi;
    if (nalloc < mp->num) nalloc = mp->num;
    mpi = reallox(mp->dat, nalloc * sizeof(mapgen));
    if (NULL == mpi) return FAILMEM;
    mp->dat = mpi;
    mp->alloc = nalloc;
    return SUCCESS;
}

void mapDestroy(map *mp) {
    int i;
    for (i=mp->num;i--;) 
        mapgenDestroy(&(mp->dat[i]));
    if (mp->alloc) 
        freex(mp->dat);
    freex(mp);
}

int compareMapGenById(const mapgen *a, const mapgen *b) {
    return a->id - b->id;
}

int mpgSortFunc (const void *aa, const void *bb) {
    return compareMapGenById((const mapgen *)aa, (const mapgen *)bb);
} 

mapgen *mapFindGen(map *mp, int id) {
    mapgen aux, *res;
    aux.id = id;
    res = bsearch(&aux, mp->dat, mp->num, sizeof(mapgen), mpgSortFunc);
    return res;
}
int mapAddGen(map *mp, int id) {
    mapgen *res;
    if (mp->num == mp->alloc) 
        mapRealloc(mp, mp->num + 10);
    res = &(mp->dat[mp->num++]);
    mapgenInit(res);
    res->id = id;
    if (id >= mp->maxgen) mp->maxgen = id + 1;
    qsort(mp->dat, mp->num, sizeof(mapgen), mpgSortFunc);
    return SUCCESS;
}

int mapGetMinIdeg(map *mp) {
    int res = 0;
    int i;
    for (i=0;i<mp->num;i++)
        if (mp->dat[i].ideg < res) 
            res = mp->dat[i].ideg;
    return res;
}

int mapGetMaxIdeg(map *mp) {
    int res = 0;
    int i;
    for (i=0;i<mp->num;i++)
        if (mp->dat[i].ideg > res) 
            res = mp->dat[i].ideg;
    return res;
}

/*==============================================================================*/

#define COMPRET(x,y) if (0 != (res = (x)-(y))) return res;
int cmpMpSI(const void *aa, const void *bb) {
    const mapsqnitem *a = (const mapsqnitem *) aa;
    const mapsqnitem *b = (const mapsqnitem *) bb;
    int res;
    COMPRET(a->gen, b->gen);
    COMPRET(a->edat, b->edat);
    return 0;
}

int mapsqndatareallox(mapsqndata *mps, int nalloc) {
    mapsqnitem *mpi;

    if (nalloc < mps->num) nalloc = mps->num;
    mpi = reallox(mps->dat, nalloc * sizeof(mapsqnitem));
    if (NULL == mpi) return FAILMEM;
    mps->dat = mpi; mps->alloc = nalloc;
    return SUCCESS;
}

mapsqnitem *newMapSqnItm(mapsqndata *mps) {
    mapsqnitem *res;
    if (mps->num >= mps->alloc) 
        if (SUCCESS != mapsqndatareallox(mps, mps->alloc + 100)) 
            return NULL;
    res = &(mps->dat[mps->num]);
    ++(mps->num); 
    printf("newMapSqnItm\n");
    return res;
}

mapsqndata *mapCreateSqnData(map *mp, enumEnv *env, int edeg, int ideg) {
    mapsqndata *res;
    int i;

    if (NULL == (res = callox(1, sizeof(mapsqndata))))
        return NULL;

    res->dat = NULL;
    res->num = res->alloc = 0;

    res->env = env;
    
    for (i=0;i<mp->num;i++) {
        mapgen *gen = &(mp->dat[i]);
        int edegrem = edeg - gen->edeg;
        int idegrem = ideg - gen->ideg;
        int edat, deg;
        printf("gen #%d edeg = %d, ideg = %d, edegrem=%d, idegrem=%d\n",
               gen->id, gen->edeg, gen->ideg, edegrem, idegrem);
        if ((edegrem < 0) || (idegrem < 0)) continue;
        /* find all exteriors in the given subalgebra that have 
         * the prescribed exterior degree edegrem */
        deg = idegrem;
        edat = getMaxExterior(env, &deg);
        for (;edat>=0;edat--) 
            if (BITCOUNT(edat) == edegrem) {
                mapsqnitem *itm;
                if (NULL == (itm = newMapSqnItm(res))) {
                    mapDestroySqnData(res);
                    return NULL;
                }
                itm->gen  = gen->id;
                itm->edat = edat;
                itm->gideg = gen->ideg;
                itm->gedeg = gen->edeg;
                printf("itm rideg = %d\n",idegrem);
                itm->rideg = idegrem;
            }
    }

    qsort(res->dat, res->num, sizeof(mapsqnitem), cmpMpSI);
    return res;
}

void mapDestroySqnData(mapsqndata *mps) {
    if (NULL != mps->dat) freex(mps->dat);
    if (NULL != mps->sqn) destroySeqno(mps->sqn);
    freex(mps);
}

/* enumeration based on mapsqndata 
 *
 * we here use the exm component of the mapsqndata struct */

int MSDfirstAux(mapsqndata *msd, exmon *sig, exmon *var) {
    int i;
    for (i=msd->exm.id;i<msd->num;i++) 
        if (firstRedmon(&(msd->exm), msd->env, msd->dat[i].rideg)) {
            var->id = msd->exm.id = msd->dat[i].gen;
            setSignature(var, msd->env, &(msd->exm), sig);
            return 1;
        }
    return 0;
}

int MSDfirst(mapsqndata *msd, exmon *sig, exmon *var) {
    msd->exm.id = 0;
    return MSDfirstAux(msd, sig, var);
}

int MSDnext(mapsqndata *msd, exmon *sig, exmon *var) {
    int i = msd->exm.id;
    if (nextRedmon(&(msd->exm), msd->env)) {
        setSignature(var, msd->env, &(msd->exm), sig);
        return 1;
    }
    var->id = msd->exm.id = i+1;
    return MSDfirstAux(msd, sig, var);
}




