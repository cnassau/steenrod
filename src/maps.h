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

#include <prime.h>

/* a sum of reduced powers with a common exterior component and generator */
typedef struct {
    int   num;    /* number of summands */
    int   len;    /* number of xint's per summand */
    int   edat;   /* exterior data */
    int   gen;    /* generator */
    int   seqoff; /* seqno offset */
    xint  pad;    /* padding value: 0 for ordinary ops, -1 for negative ops */  
    xint *dat;    /* the data */
} diffsum;

/* an array of diffsum */
typedef struct {
    int num;   /* number of used diffsum's */
    int alloc; /* number of allocated diffsum's */
    diffsum *dat; 
} diff;

/* number of diffsum to add whenever we run out of space */
#define MAPGROWSTEP 100

#endif



