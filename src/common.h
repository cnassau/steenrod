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

#define DONT_USE_TCL_ALLOC 
#define USE_VERB_ALLOC 

#define vbfree(p) (printf("freeing %p\n",p), free(p))
#define vbmalloc(s) \
({ void *_res = malloc(s); \
printf("malloc'ed %u bytes at %p\n",(unsigned) s, _res); \
_res ;})

/* allocation wrappers */
#ifndef mallox 
#  ifdef USE_VERB_ALLOC
#    define mallox(s)    vbmalloc(s)
#    define callox(n,s)  calloc(n,s)
#    define reallox(p,s) realloc(p,s)
#    define freex(p)     vbfree(p) 
#  else 
#    ifdef USE_TCL_ALLOC
#      include <tcl.h>
#      define mallox(s)    ((void *) ckalloc(s))
#      define callox(n,s)  ((void *) ckalloc((n)*(s)))
#      define reallox(p,s) ((void *) ckrealloc((void *) p,s))
#      define freex(p)     (ckfree((void *) p)) 
#    else
#      define mallox(s)    malloc(s)
#      define callox(n,s)  calloc(n,s)
#      define reallox(p,s) realloc(p,s)
#      define freex(p)     free(p) 
#    endif
#  endif
#endif

#endif
