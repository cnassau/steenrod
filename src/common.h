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

/* #define USE_TCL_ALLOC */

#define myfree(p) (printf("freeing %p\n",p), free(p))

/* allocation wrappers */
#ifndef mallox 
#  ifndef USE_TCL_ALLOC
#    define mallox(s)    malloc(s)
#    define callox(n,s)  calloc(n,s)
#    define reallox(p,s) realloc(p,s)
#    define freex(p)     myfree(p) 
#  else
#    include <tcl.h>
#    define mallox(s)    ((void *) ckalloc(s))
#    define callox(n,s)  ((void *) ckalloc((n)*(s)))
#    define reallox(p,s) ((void *) ckrealloc((void *) p,s))
#    define freex(p)     (ckfree((void *) p)) 
#endif
#endif

#endif
