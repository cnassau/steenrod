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

#define NALG 9

/* the following data type is the core of several other types:
 * profiles, subalgebras, extmonos. It consists of an exponent 
 * sequence rdat (r = reduced) and a bitmask edat (e = exterior). */

typedef struct {
    int edat;
    int rdat[NALG];
} procore;

/* a profile */
typedef struct {
    procore core; 
} profile;

/* description of a sub Hopf-algebra */
typedef struct {
    procore core;     
} subalg;

/* extended monomials [= monomials with some (invisible) extra stuff] */
typedef struct {
    procore core;     
} emon; 

#endif
