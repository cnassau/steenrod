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

/* namespaces */
#define POLYNSP "steenrod::"
#define MONONSP "steenrod::"

/* error codes */
#define SUCCESS         0 
#define FAIL            1  /* general failure */
#define FAILMEM         2  /* out of memory */
#define FAILIMPOSSIBLE  3  /* operation not possible */
#define FAILUNTRUE      4  /* another word for "no" */

#define DONT_USE_TCL_ALLOC 

/* allocation wrappers */
#ifndef mallox 
#    ifdef USE_TCL_ALLOC
#      include <tcl.h>
#      define mallox(s)    ((void *) ckalloc(s))
#      define callox(n,s)  \
({size_t _sz = (s)*(n); char *_res = ckalloc(_sz); \
if (_res) memset(_res, 0,_sz); (void *) _res; })
#      define reallox(p,s) ((void *) ckrealloc((void *) p,s))
#      define freex(p)     (ckfree((void *) p)) 
#    else
#      define mallox(s)    malloc((s))
#      define callox(n,s)  calloc(n,(s))
#      define reallox(p,s) realloc(p,(s))
#      define freex(p)     free(p) 
#    endif
#endif

#define MIN(x,y) (((x)>(y)) ? (y) : (x))
#define MAX(x,y) (((x)>(y)) ? (x) : (y))

/* the following trick is from the cpp info page on stringification: */
#define TOSTRING(s) _cxstr(s)
#define _cxstr(s) #s


#endif
