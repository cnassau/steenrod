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

#ifndef MAPS_DEF
#define MAPS_DEF

#include <tcl.h>
#include <common.h>
#include <prime.h>
#include <profile.h>

/* a sum of reduced powers with a common exterior component and generator */
typedef struct {
    int   num;    /* number of used summands */
    int   alloc;  /* number of allocated summands */
    int   edat;   /* exterior data */
    int   gen;    /* generator */
    int   len;    /* number of xint's per summand */
    xint  pad;    /* padding value: 0 for ordinary ops, -1 for negative ops */  
    cint *cdat;   /* the coefficients */
    xint *xdat;   /* the exponent data */
} mapsum;

/* image of a generator under the map = an array of mapsum */
typedef struct {
    /* description of the generator: */
    int id;          /* numerical id */
    int edeg, ideg;  /* exterior and internal degree */
    /* description of its image: */
    int num;         /* number of used mapsum's */
    int alloc;       /* number of allocated mapsum's */
    mapsum *dat; 
} mapgen;

/* number of mapsum to add if we run out of space */
#define MAPGROWSTEP 100

/* finally, the map is a collection of mapgen's */
typedef struct {
    int num; 
    int alloc;
    mapgen *dat;
    int maxgen;   /* suggested number for next generator */
} map;

void mapsumInit(mapsum *mps);
void mapsumDestroy(mapsum *mp);
int mapsumSetLen(mapsum *mp, int len);
int mapsumSetPad(mapsum *mp, int pad);
int mapsumRealloc(mapsum *mp, int nalloc);
int mapsumAppendFromList(mapsum *mps, int len, int obc, Tcl_Obj **obv);

void mapgenInit(mapgen *mpi);
int mapgenRealloc(mapgen *mim, int nalloc);
void mapgenDestroy(mapgen *mim);

mapsum *mapgenFindSum(mapgen *mpg, int gen, int edat);
mapsum *mapgenCreateSum(mapgen *mpg, int gen, int edat);

map *mapCreate(void);
int mapRealloc(map *mp, int nalloc);
void mapDestroy(map *mp);

mapgen *mapFindGen(map *mp, int id);
int mapAddGen(map *mp, int id);

int mapGetMinIdeg(map *mp);
int mapGetMaxIdeg(map *mp);

/* an array of these items is used by sequence number computations */
typedef struct {
    int gen;    /* generator id */
    int edat;   /* exterior data */
    int gideg;  /* internal degree of the generator */
    int gedeg;  /* external degree of the generator */
    int seqoff; /* sequence number offset */
    int rideg;  /* remaining internal degree = i.deg - gideg */
} mapsqnitem;

typedef struct {
    /* an array of mapsqnitems */
    int num;
    int alloc;
    mapsqnitem *dat;
    /* other enumeration relevant data */
    exmon      exm;
    enumEnv   *env;
    seqnoInfo *sqn;
} mapsqndata;

/* creation and destruction */
mapsqndata *mapCreateSqnData(map *mp, enumEnv *env, int edeg, int ideg);
void mapDestroySqnData(mapsqndata *mps);

/* enumeration based on mapsqndat */
int MSDfirst(mapsqndata *msd, exmon *sig, exmon *var); 
int MSDnext(mapsqndata *msd, exmon *sig, exmon *var); 

#endif
