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

#ifndef ENUM_DEF
#define ENUM_DEF

#include "poly.h"

/* effgen = effective generator; used internally in the enumerator structure */
typedef struct {
    int id;     /* generator id */
    int ext;    /* exterior component */
    int rrideg; /* reduced, remaining internal degree */
} effgen;

typedef struct {
    /* description of the algebra/subalgebra pair */
    exmo       algebra, profile;
    primeInfo *pi;

    /* genList is a list of 4-tuples (gen-id, ideg, edeg, hdeg) */
    int       *genList, numgens;
    int       idegmin, edegmin, maxgenid;

    /* the tri-degree that we're enumerating */
    int       ideg, edeg, hdeg;

    /* the monomial that is used for enumeration + helper ints */
    int gencnt; /* number of generator in the efflist */
    exmo varex;
    int errdeg;
    int totdeg;
    int remdeg;
    int extdeg;

    /* tables that are used for sequence number computations */
    int effdeg[NALG];      /* this is "reddeg * profile" */
    int tablen;            /* length of the following arrays */
    int *(dimtab[NALG+1]); 
    int *(seqtab[NALG+1]); 

    /* list of effective generators, and their sequence number offsets */
    effgen    *efflist;
    int       *seqoff;
    int        efflen; 

} enumerator;

enumerator *enmCreate(void);
void        enmDestroy(enumerator *en);

#ifndef ENUMC
extern polyType enumPolyType;
#endif

#define enumpoly (&(enumPolyType)) 

#endif

