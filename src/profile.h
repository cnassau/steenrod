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

#ifndef PROFILE_DEF
#define PROFILE_DEF

#include "prime.h"

/* here NPRO is the maximum length of exponent sequences: */
#define NPRO 9

/* the following data type is the core of several other types:
 * profiles, subalgebras, extmonos. It consists of an exponent 
 * sequence rdat (r = reduced) and a bitmask edat (e = exterior). */

typedef struct {
    int edat;
    int rdat[NPRO];
} procore;

/* extended monomials [= monomials with a few helper fields] */
typedef struct {
    procore core; 
    int errdeg; 
    int totdeg;
    int remdeg;
    int extdeg;
} exmon; 

/* the profile of a sub Hopf-algebra */
typedef struct {
    procore core;
} profile;

void makeZeroProfile(profile *pro);
void makeFullProfile(profile *pro, primeInfo *pi, int maxdim);

/* information that's needed by the seqno routine */
typedef struct {
    int maxdim;    /* maximal dimension for the tables below */
    int *(dimtab[NPRO+1]); 
    int *(seqtab[NPRO+1]);
} seqnoInfo;

/* The environment for enumeration purposes: we have two nested sub 
 * Hopf-algebras *pro <= *alg and the assumption is that enumeration 
 * is restricted to elements of *alg that have a fixed *pro signature. */
typedef struct {
    primeInfo *pi;  
    profile *alg;
    profile *pro;
    profile *seqnoInfo;
} enumEnv;

/* constructor & destructor */
enumEnv *createEnumEnv(primeInfo *pi, profile *alg, profile *pro);
void disposeEnumEnv(enumEnv *env);

seqnoInfo *createSeqno(primeInfo *pi, profile *alg, profile *pro, int maxdim);
void disposeSeqno(seqnoInfo *s);

/* enumeration of exmons of a given degree */
int firstExmon(exmon *ex, enumEnv *env, int deg);
int nextExmon(exmon *ex, enumEnv *env, int deg);

/* enumeration again, this time restricted to the reduced part */
int firstRedmon(exmon *ex, enumEnv *env, int deg);
int nextRedmon(exmon *ex, enumEnv *env);

#endif
