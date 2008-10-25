/*
 * All about enumeration and sequence numbers
 *
 * Copyright (C) 2004-2009 Christian Nassau <nassau@nullhomotopie.de>
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
    exmo       algebra, profile, signature;
    primeInfo *pi;

    int ispos;  /* whether this enumerates positive or negative Steenrod ops */

    /* genList is a list of 4-tuples (gen-id, ideg, edeg, hdeg) */
    int       *genList, numgens;
    int       maxideg, minideg, maxedeg, minedeg, minhdeg, maxhdeg, mingen, maxgen;

    /* the tri-degree that we're enumerating */
    int       ideg, edeg, hdeg;

    /* the monomial that is used for enumeration + helper ints */
    int gencnt; /* number of generator in the efflist */
    int yacntr; /* yet another counter - used in the enumpoly routines */
    exmo varex;
    exmo theex; 
    int errdeg;
    int totdeg;
    int remdeg;
    int extdeg;
    int sigideg, sigedeg; 

    /* tables that are used for sequence number computations */
    int effdeg[NALG];      /* this is "reddeg * profile" */
    int tablen;            /* length of the following arrays */
    int tabmaxrideg;       /* maximal reduced internal degree */
    int *(dimtab[NALG+1]); 
    int *(seqtab[NALG+1]); 
    int totaldim;

    /* list of effective generators, and their sequence number offsets */
    effgen    *efflist;
    int       *seqoff;
    int        efflen, effalloc;
    int        maxrrideg, maxredeg;  

} enumerator;

enumerator *enmCreate(void);
enumerator *enmCopy(enumerator *src);
void        enmDestroy(enumerator *en);

/* We use the lazy approach for the configuration of an enumerator, so 
 * setting an option to a new value just invalidates all dependent fields;
 * those are then recomputed later on demand. 
 *
 * Options are grouped into the following blocks:
 *
 *  - basics:       prime, algebra, profile 
 *  - trideg:       ideg, edeg, hdeg, signature
 *  - genlist:      genlist
 */

int enmSetBasics(enumerator *en, primeInfo *pi, 
                 exmo *algebra, exmo *profile, int ispos);
int enmSetSignature(enumerator *en, exmo *sig);
int enmSetTridegree(enumerator *en, int ideg, int edeg, int hdeg);
int enmSetGenlist(enumerator *en, int *gl, int num);

/* enumeration uses these procedures. the enumerated exmo is "theex". */
int firstRedmon(enumerator *en);
int nextRedmon(enumerator *en);

int SeqnoFromEnum(enumerator *en, exmo *ex);
int DimensionFromEnum(enumerator *en);

/* signature enumeration; the first signature is always zero */
int nextSignature(enumerator *en, exmo *sig, int *sideg, int *sedeg);
int enmIncrementSig(enumerator *en);

/* An enumpoly allows to access an enumerator as a polynomial */

#ifndef ENUMC
extern polyType enumPolyType;
#endif

#define enumpoly (&(enumPolyType)) 

#endif

