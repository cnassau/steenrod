/*
 * Some routines for sse2 optimization
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

#ifndef SSEDEFS
#define SSEDEFS

#ifndef USESSE2
#  error "ssedefs.h included but USESSE2 not set"
#endif

// #define DEBUGSSE2

#include "linalg.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static inline
void print_epi8(const char *name, __m128i reg) {
    int i; const char *c;
    union {
        __m128i aux;
        unsigned char dat[16];
    } x;
    x.aux = reg;
    printf("%10s",name);
    for(i=0,c=" = ";i<16;i++,c=", ") 
        printf("%s%d",c,x.dat[i]);
    printf("\n");
}

static inline
void print_epi16(const char *name, __m128i reg) {
    int i; const char *c;
    union {
        __m128i aux;
        unsigned short dat[8];
    } x;
    x.aux = reg;
    printf("%10s",name);
    for(i=0,c=" = ";i<8;i++,c=", ") 
        printf("%s%d",c,x.dat[i]);
    printf("\n");
}

#ifdef DEBUGSSE2
#  define PRINTMSG(text)      { printf(text "\n"); }
#  define PRINTEPI8(varname)  { print_epi8(#varname,varname);  }
#  define PRINTEPI16(varname) { print_epi16(#varname,varname); }
#else
#  define PRINTMSG(text)      { ; }
#  define PRINTEPI8(varname)  { ; }
#  define PRINTEPI16(varname) { ; }
#endif

static inline 
void set_entry(__m128i *var, unsigned int idx, char newval) {
    union {
        __m128i aux;
        char chs[16];
    } x;
    x.aux = *var;
    x.chs[idx & 15]=newval;
    *var = x.aux;
}

static inline 
char extract_entry(__m128i var, unsigned int idx) {
    union {
        __m128i aux;
        char chs[16];
    } x;
    x.aux = var;
    return x.chs[idx & 15];
}

static inline 
void add_blocks(__m128i *dst,__m128i *src, int nblocks, 
                int coeff, int p) {
    PRINTMSG("======== add_blocks ==========");
    if (p) { coeff %= p; if (coeff<0) coeff += p; }
    __m128i scale = _mm_set1_epi16(coeff);
    __m128i prime = _mm_set1_epi16(p << 5);
    PRINTEPI16(scale);PRINTEPI16(prime);
    while (nblocks--) {
        PRINTMSG("      === new block ===");
        __m128i srcl = *src, srch = _mm_xor_si128(srcl,srcl);
        srcl = _mm_unpacklo_epi8(srcl,srch);
        srch = _mm_unpackhi_epi8(*src,srch);
        PRINTEPI8(*src); PRINTEPI16(srcl); PRINTEPI16(srch);
        {
            __m128i dstl = _mm_unpacklo_epi8(*dst,_mm_setzero_si128());
            srcl = _mm_add_epi16(dstl,_mm_mullo_epi16(scale,srcl));
        }
        {
            __m128i dsth = _mm_unpackhi_epi8(*dst,_mm_setzero_si128());
            srch = _mm_add_epi16(dsth,_mm_mullo_epi16(scale,srch));
        }
#define REDUCTIONBLOCK(dummy)                                         \
        {                                                             \
            PRINTEPI16(prime); PRINTEPI16(srcl); PRINTEPI16(srch);    \
            __m128i maskl =  _mm_cmpgt_epi16(prime,srcl);             \
            __m128i maskh =  _mm_cmpgt_epi16(prime,srch);             \
            PRINTEPI16(maskl); PRINTEPI16(maskh);                     \
            srcl = _mm_sub_epi16(srcl,_mm_andnot_si128(maskl,prime)); \
            srch = _mm_sub_epi16(srch,_mm_andnot_si128(maskh,prime)); \
        }
        REDUCTIONBLOCK(p<<5); prime = _mm_srli_epi16(prime,1);
        REDUCTIONBLOCK(p<<4); prime = _mm_srli_epi16(prime,1);
        REDUCTIONBLOCK(p<<3); prime = _mm_srli_epi16(prime,1);
        REDUCTIONBLOCK(p<<2); prime = _mm_srli_epi16(prime,1);
        REDUCTIONBLOCK(p<<1); prime = _mm_srli_epi16(prime,1);
        REDUCTIONBLOCK(p);
        prime = _mm_slli_epi16(prime,5);
        *dst = _mm_packs_epi16(srcl,srch);
        PRINTEPI8(*dst); PRINTEPI16(srcl); PRINTEPI16(srch);
        dst++,src++;
    }
}

static inline 
void reduce_blocks(__m128i *dt, int numblocks, int prime) {
    __m128i p16 = _mm_set1_epi8(prime<<4);
    __m128i p8  = _mm_set1_epi8(prime<<3);
    __m128i p4  = _mm_set1_epi8(prime<<2);
    __m128i p2  = _mm_set1_epi8(prime<<1);
    __m128i p1  = _mm_set1_epi8(prime);
    for(;numblocks--;) {
        __m128i val = *dt, mask;
        mask = _mm_cmpgt_epi8(val,p16);
        val = _mm_sub_epi8(val,_mm_and_si128(mask,p16));
        mask = _mm_cmpgt_epi8(val,p8);
        val = _mm_sub_epi8(val,_mm_and_si128(mask,p8));
        mask = _mm_cmpgt_epi8(val,p4);
        val = _mm_sub_epi8(val,_mm_and_si128(mask,p4));
        mask = _mm_cmpgt_epi8(val,p2);
        val = _mm_sub_epi8(val,_mm_and_si128(mask,p2));
        mask = _mm_cmpgt_epi8(val,p1);
        *dt = _mm_sub_epi8(val,_mm_and_si128(mask,p1));
    }
}

#endif /* defined SSEDEFS */
 
