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

#include "maps.h"

mapsum *mapsumCreate(void) {
    mapsum *res = malloc(sizeof(mapsum));
    if (NULL == res) return NULL;
    res->num = res->alloc = 0;
    res->len = 0; 
    res->cdat = NULL; res->xdat = NULL;
    return res;
}

void mapsumDestroy(mapsum *mp) {
    if (0 != mp->alloc) {
        free(mp->cdat); 
        free(mp->xdat);
    }
    free(mp);
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

mapgenimage *mapgenimageCreate(void) {
    mapgenimage *res = malloc(sizeof(mapgenimage));
    if (NULL == res) return NULL;
    res->num = res->alloc = 0; 
    res->dat = NULL;
    return res;
}

int mapgenimageRealloc(mapgenimage *mim, int nalloc) {
    mapsum *ndat; 
    if (nalloc < mim->num) nalloc = mim->num;
    ndat = realloc(mim->dat, nalloc * sizeof(mapsum)); 
    if (NULL == ndat) return FAILMEM;
    mim->alloc = nalloc;
    return SUCCESS;
}
  
map *mapCreate(void) {
    map *res = malloc(sizeof(map));
    if (NULL == res) return NULL;
    res->num = res->alloc = 0;
    return res;
}

int mapRealloc(map *mp, int nalloc) {
    mapgenimage *mpi;
    if (nalloc < mp->num) nalloc = mp->num;
    mpi = realloc(mp->dat, nalloc * sizeof(mapgenimage));
    if (NULL == mpi) return FAILMEM;
    mp->dat = mpi;
    mp->alloc = nalloc;
    return SUCCESS;
}
