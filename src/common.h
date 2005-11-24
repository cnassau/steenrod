/*
 * Common macros, typedefs, constants, etc.
 *
 * Copyright (C) 2004 Christian Nassau <nassau@nullhomotopie.de>
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

/* undef HAVE_ASSERT if you're seeing "eprintf" related problems with gcc */
#define HAVE_ASSERT

#ifdef HAVE_ASSERT
#  include <assert.h>
#  define ASSERT(x) assert(x) 
#else
#  define ASSERT(x) { /* do nothing */ }
#endif

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
#define DONT_USE_VERB_ALLOC

/* allocation wrappers */
#ifndef mallox 
#  ifdef USE_TCL_ALLOC
#    include <tcl.h>
#    define mallox(s)    ((void *) ckalloc(s))
#    define callox(n,s)  \
({size_t _sz = (s)*(n); char *_res = ckalloc(_sz); \
if (_res) memset(_res, 0,_sz); (void *) _res; })
#    define reallox(p,s) ((void *) ckrealloc((void *) p,s))
#    define freex(p)     (ckfree((void *) p)) 
#  else
#    ifdef USE_VERB_ALLOC
#      include <stdio.h>
#      define mallox(s)    vbmalloc((s),__FILE__,__LINE__)
#      define callox(n,s)  vbcalloc(n,(s),__FILE__,__LINE__)
#      define reallox(p,s) vbrealloc(p,(s),__FILE__,__LINE__)
#      define freex(p)     vbfree(p,__FILE__,__LINE__)
#    else 
#      define mallox(s)    malloc((s))
#      define callox(n,s)  calloc(n,(s))
#      define reallox(p,s) realloc(p,(s))
#      define freex(p)     free(p)
#    endif 
#  endif
#endif


/* verbatim memory management - used for debugging */
#define _MEMLOG(sym,addr,oad,siz,file,line) \
{ fprintf(stderr,"MDBG %s %p %p %d %s %d\n",sym,addr,oad,siz,file,line); }

#define vbmalloc(siz,file,line) \
({ size_t _sz = siz; void *_res = malloc(_sz); _MEMLOG("M",_res,NULL,_sz,file,line); _res;}) 

#define vbcalloc(cnt,siz,file,line) \
({ size_t _sz = siz; void *_res = calloc(cnt,_sz); _MEMLOG("C",_res,NULL,_sz,file,line); _res;}) 

#define vbrealloc(ptr, siz,file,line) \
({ size_t _sz = siz; void *_res = realloc(ptr,_sz); _MEMLOG("R",_res,ptr,_sz,file,line); _res;}) 

#define vbfree(ptr,file,line) \
({ void *_ptr = ptr; free(_ptr); _MEMLOG("F",NULL,_ptr,0,file,line);}) 


#define MIN(x,y) (((x)>(y)) ? (y) : (x))
#define MAX(x,y) (((x)>(y)) ? (x) : (y))

/* the following trick is from the cpp info page on stringification: */
#define TOSTRING(s) _cxstr(s)
#define _cxstr(s) #s

#ifndef COMMONC
extern int objCount; /* for refCount debgging */
#endif

#define DONT_USE_VERB_REFCOUNTS
#define DONT_COUNT_REFCOUNTS

/* wrappers for Tcl reference count management */
#ifndef USE_VERB_REFCOUNTS
#  ifndef COUNT_REFCOUNTS
#    define INCREFCNT(x) Tcl_IncrRefCount(x)
#    define DECREFCNT(x) Tcl_DecrRefCount(x)
#  else 
#    define INCREFCNT(x) { ++objCount; Tcl_IncrRefCount(x); }
#    define DECREFCNT(x) { --objCount; Tcl_DecrRefCount(x); }
#  endif
#else 
#  define INCREFCNT(x) \
{ fprintf(stderr, "increfcnt %p %d " __FILE__ " %d {%s}\n", (x), ((x)->refCount), \
      __LINE__, (NULL == (x)->typePtr) ? "untyped" : (x)->typePtr->name); \
  Tcl_IncrRefCount(x); } 
#  define DECREFCNT(x) \
{ fprintf(stderr, "decrefcnt %p %d " __FILE__ " %d {%s}\n", (x), ((x)->refCount), \
      __LINE__, (NULL == (x)->typePtr) ? "untyped" : (x)->typePtr->name); \
  Tcl_DecrRefCount(x); } 
#endif


#endif
