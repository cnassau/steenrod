/*
 * Common macros, typedefs, constants, etc.
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

#ifndef COMMON_DEF
#define COMMON_DEF

#include <assert.h>

/* error codes */
#define SUCCESS         0 
#define FAIL            1  /* general failure */
#define FAILMEM         2  /* out of memory */
#define FAILIMPOSSIBLE  3  /* operation not possible */
#define FAILUNTRUE      4  /* another word for "no" */

/* allocation wrappers */
#ifndef mallox 
#  define mallox  malloc
#  define reallox realloc
#  define freex   free
#endif

#endif
