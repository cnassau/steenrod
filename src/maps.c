/*
 * Representation of a map from one free module to another
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
#include "maps.h"

void mapsumInit(mapsum *res) {
    res->num = res->alloc = 0;
    res->len = 0; 
    res->cdat = NULL; res->xdat = NULL;
}

void mapsumDestroy(mapsum *mp) {
    if (0 != mp->alloc) {
        free(mp->cdat); 
        free(mp->xdat);
    }
}

int mapsumSetlen(mapsum *mp, int len) {
    xint *nxd, *src, *dst; 
    int i, j, min, max;
    if (mp->len == len) return SUCCESS;
    if (0 == mp->num) {
        if (0!=mp->alloc) { free(mp->xdat); free(mp->cdat); }
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
    nxd = malloc(len * mp->alloc * sizeof(xint));
    if (NULL == nxd) return FAILMEM;
    for (dst=nxd,src=mp->xdat,i=mp->num;i--;dst+=len,src+=mp->len) {
      for (j=0;j<min;j++) dst[j] = src[j];
      for (j=min;j<max;j++) dst[j] = mp->pad;
    }
    mp->len = len;
    free(mp->xdat);
    mp->xdat = nxd;
    return SUCCESS;
}

int mapsumRealloc(mapsum *mp, int nalloc) {
    cint *ncd; xint *nxd;
    if (nalloc < mp->num) nalloc = mp->num;
    if (NULL == (ncd = realloc(mp->cdat, nalloc * sizeof(cint)))) {
        return FAILMEM;
    }
    mp->cdat = ncd;
    if (NULL == (nxd = realloc(mp->xdat, mp->len * nalloc * sizeof(xint)))) {
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

mapgen *mapgenCreate(void) {
    mapgen *res = malloc(sizeof(mapgen));
    if (NULL == res) return NULL;
    res->num = res->alloc = 0; 
    res->dat = NULL;
    return res;
}

int mapgenRealloc(mapgen *mim, int nalloc) {
    mapsum *ndat; 
    if (nalloc < mim->num) nalloc = mim->num;
    ndat = realloc(mim->dat, nalloc * sizeof(mapsum)); 
    if (NULL == ndat) return FAILMEM;
    mim->alloc = nalloc;
    return SUCCESS;
}
  
void mapgenDestroy(mapgen *mim) {
    int i;
    for (i=mim->num;i--;)
        mapsumDestroy(&(mim->dat[i]));
    if (mim->alloc)
        free(mim->dat);
}

mapsum *mapgenAddSum(mapgen *mpi, int id, int edat) {
    mapsum aux, *res;
    aux.gen = id;
    aux.edat = edat;
    res = bsearch(&aux, mpi->dat, mpi->num, sizeof(mapsum), mapsumCompFunc);
    return res;
}

map *mapCreate(void) {
    map *res = malloc(sizeof(map));
    if (NULL == res) return NULL;
    res->num = res->alloc = 0; res->dat = NULL;
    return res;
}

int mapRealloc(map *mp, int nalloc) {
    mapgen *mpi;
    if (nalloc < mp->num) nalloc = mp->num;
    mpi = realloc(mp->dat, nalloc * sizeof(mapgen));
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
        free(mp->dat);
    free(mp);
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
    qsort(mp->dat, mp->num, sizeof(mapgen), mpgSortFunc);
    return SUCCESS;
}
