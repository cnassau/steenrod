/*
 * Monomials, polynomials, and basic operations
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

#define POLYC

#include <stdlib.h>
#include <string.h>
#include "poly.h"
#include "common.h"

#define LOGSTD(msg) if (0) printf("stdpoly::%s\n", msg) 

/**** extended monomials ***********************************************************/

int exmoGetLen(exmo *e) {
    int pad = exmoGetPad(e), j;
    for (j=NALG;j--;) 
        if (e->dat[j] != pad) 
            return (j+1);
    return 0;
}

int exmoGetPad(exmo *e) {
    return (e->dat[0]<0) ? -1 : 0;
}

void copyExmo(exmo *dest, exmo *src) {
    memcpy(dest,src,sizeof(exmo));
}

void shiftExmo(exmo *e, const exmo *s, int flags) {
    int i;
    for (i=NALG;i--;) 
        e->dat[i] += s->dat[i];
    /* TODO: signs not yet implemented */
    if (0 != (flags & ADJUSTSIGNS)) 
        if (0 != (e->ext & s->ext)) e->coeff = 0;
    e->ext ^= s->ext;
}

void reflectExmo(exmo *e) {
    int i;
    for (i=NALG;i--;) e->dat[i] = -1 - e->dat[i];
    e->ext = -1 - e->ext;
}

#define COMPRET(x,y) if (0 != (diff = ((x)-(y)))) return diff;
int compareExmo(const void *aa, const void *bb) {
    int diff, i;
    const exmo *a = (const exmo *) aa;
    const exmo *b = (const exmo *) bb;
    COMPRET(a->gen,b->gen);
    COMPRET(a->ext,b->ext);
    for (i=0;i<NALG;i++)
        COMPRET(a->dat[i],b->dat[i]);
    return 0;
}

/**** generic polynomials *********************************************************/

#define CALLIFNONZERO1(func,arg1) \
if (NULL != (func)) (func)(arg1);

int PLgetLength(polyType *type, void *poly) {
    return (type->getLength)(poly);
}

void PLfree(polyType *type, void *poly) { 
    CALLIFNONZERO1(type->free,poly); 
}

int PLclear(polyType *type, void *poly) { 
    CALLIFNONZERO1(type->clear,poly) else return FAILIMPOSSIBLE;
    return SUCCESS;
}

void *PLcreate(polyType *type) {
    void *res = NULL;
    if (NULL != type->createCopy) 
        res = (type->createCopy)(NULL);
    return res;
}

int PLcancel(polyType *type, void *poly, int modulo) { 
    if (NULL != type->cancel) {
        (type->cancel)(poly,modulo);
        return SUCCESS;
    }
    return FAILIMPOSSIBLE;
}

int PLgetExmo(polyType *type, void *self, exmo *ex, int index) {
    exmo *aux;
    if (NULL != type->getExmo) return (type->getExmo)(self, ex, index);
    if (NULL != type->getExmoPtr) 
        if (SUCCESS == (type->getExmoPtr)(self, &aux, index)) {
            copyExmo(ex,aux);
            return SUCCESS;
        }
    return FAILIMPOSSIBLE;
}

int PLappendExmo(polyType *dtp, void *dst, exmo *e) {
    if (NULL != dtp->appendExmo) return (dtp->appendExmo)(dst,e);
    return FAILIMPOSSIBLE;
}

int PLappendPoly(polyType *dtp, void *dst, 
                 polyType *stp, void *src,     
                 const exmo *shift,
                 int flags,
                 int scale, int modulo) {
    exmo e; int i, len;
    if ((dtp == src) && (NULL != dtp->appendPoly))
        return (dtp->appendPoly)(dst,src,shift,flags,scale,modulo);
    len = (stp->getLength)(src);
    if (modulo) scale %= modulo;
    for (i=0;i<len;i++) {
        if (SUCCESS != PLgetExmo(stp,src,&e,i)) 
            return FAILIMPOSSIBLE;
        if (NULL != shift) shiftExmo(&e,shift,flags);
        e.coeff *= scale; if (modulo) e.coeff %= modulo;
        if (SUCCESS != PLappendExmo(dtp,dst,&e)) 
            return FAILIMPOSSIBLE;
    }
    return SUCCESS;
}

/**** standard polynomial type ****************************************************/

/* The naive standard implementation of polynomials is as an array 
 * of exmo. This is represented by the stp structure. */

typedef struct {
    int num, nalloc;
    exmo *dat;
} stp;

void *stdCreateCopy(void *src) {
    stp *s = (stp *) src, *n;
    LOGSTD("CreateCopy");
    if (NULL == (n = (stp *) malloc(sizeof(stp)))) return NULL;
    if (NULL == s) {
        n->num = n->nalloc = 0; n->dat = NULL;
        return n;
    }
    if (NULL == (n->dat = (exmo *) malloc(sizeof(exmo) * s->num))) {
        free(n); return NULL; 
    }
    n->num = n->nalloc = s->num;
    memcpy(n->dat,s->dat,sizeof(exmo) * s->num);
    return n;
}

void stdFree(void *self) { 
    stp *s = (stp *) self;
    LOGSTD("Free");
    if (s->nalloc) { s->nalloc = s->num = 0; free(s->dat); }
    free(s);
}

void stdSwallow(void *self, void *other) { 
    stp *s = (stp *) self;
    stp *o = (stp *) other;
    LOGSTD("Swallow");
    if (s == o) return;
    stdFree(self); 
    memcpy(s,o,sizeof(stp));
    o->num = o->nalloc = 0; o->dat = NULL;
}

void stdClear(void *self) {
    stp *s = (stp *) self;
    LOGSTD("Clear");
    s->num = 0; 
}

int stdRealloc(void *self, int nalloc) {
    stp *s = (stp *) self;
    exmo *ndat;
    LOGSTD("Realloc");
    if (nalloc < s->num) nalloc = s->num;
    if (NULL == (ndat = realloc(s->dat,sizeof(exmo) * nalloc))) 
        return FAILMEM;
    s->dat = ndat; s->nalloc = nalloc;
    return SUCCESS;
}

void stdSort(void *self) {
    stp *s = (stp *) self;
    LOGSTD("Sort");
    qsort(s->dat,s->num,sizeof(exmo),compareExmo);
}

void stdCancel(void *self, int mod) {
    stp *s = (stp *) self;
    int i,j,k; double d;
    LOGSTD("Cancel");
    stdSort(self);
    for (k=i=0,j=0;i<s->num;) 
        if (((j+1)<s->num) && (0==compareExmo(&(s->dat[i]),&(s->dat[j+1])))) {
            s->dat[i].coeff += s->dat[j+1].coeff;
            if (mod) s->dat[i].coeff %= mod;
            j++;
        } else {
            int oval = s->dat[i].coeff;
            if (mod) s->dat[i].coeff %= mod;
            if (0!=s->dat[i].coeff) {
                if ((k!=i) || (oval != s->dat[i].coeff)) 
                    copyExmo(&(s->dat[k]),&(s->dat[i]));
                k++;
            }
            i=j+1; j=i; 
        }
    s->num = k;
    if (s->nalloc) { 
        d = s->num; d /= s->nalloc; 
        if (0.8 > d) stdRealloc(self, s->num * 1.1);
    } 
}

int stdCompare(void *pol1, void *pol2, int *res) {
    stp *s1 = (stp *) pol1;
    stp *s2 = (stp *) pol2;
    LOGSTD("Compare");
    stdCancel(pol1,0); stdCancel(pol2,0);
    if (s1->num != s2->num) { 
        *res = s1->num - s2->num;
        return SUCCESS;
    }
    *res = memcmp(s1->dat,s2->dat,sizeof(exmo) * s1->num);
    return SUCCESS;
}

void stdReflect(void *self) {
    stp *s = (stp *) self;
    int i;
    LOGSTD("Reflect");
    for (i=0;i<s->num;i++) 
        reflectExmo(&(s->dat[i]));
}

void stdShift(void *self, const exmo *ex, int flags) {
    stp *s = (stp *) self;
    int i;
    LOGSTD("Reflect");
    for (i=0;i<s->num;i++) 
        shiftExmo(&(s->dat[i]), ex, flags);
}

int stdGetExmoPtr(void *self, exmo **ptr, int idx) {
    stp *s = (stp *) self;
    LOGSTD("GetExmoPtr");
    if ((idx < 0) || (idx >= s->num)) return FAILIMPOSSIBLE;
    *ptr = &(s->dat[idx]);
    return SUCCESS;
} 

int stdGetLength(void *self) {
    stp *s = (stp *) self;
    LOGSTD("GetLength");
    return s->num;
}

int stdAppendExmo(void *self, exmo *ex) {
    stp *s = (stp *) self;
    LOGSTD("AppendExmo");
    if (s->num == s->nalloc) {
        int aux = s->nalloc + 10;
        if (SUCCESS != stdRealloc(self, s->nalloc + ((aux > 200) ? 200: aux)))
            return FAILMEM;
    }
    copyExmo(&(s->dat[s->num++]),ex);
    return SUCCESS;
}

int stdScaleMod(void *self, int scale, int modulo) {
    stp *s = (stp *) self;
    int i;
    LOGSTD("ScaleMod");
    if (modulo) scale %= modulo;
    for (i=0;i<s->num;i++) {
        exmo *e = &(s->dat[i]);
        e->coeff *= scale; if (modulo) e->coeff %= modulo;
    }
    return SUCCESS;
}

/* GCC extension alert! (Designated Initializers) 
 *
 * see http://gcc.gnu.org/onlinedocs/gcc-3.2.3/gcc/Designated-Inits.html */

struct polyType stdPolyType = {
    .createCopy = &stdCreateCopy,
    .free       = &stdFree,
    .swallow    = &stdSwallow,
    .clear      = &stdClear,
    .cancel     = &stdCancel,
    .compare    = &stdCompare,
    .reflect    = &stdReflect,
    .getExmoPtr = &stdGetExmoPtr,
    .getLength  = &stdGetLength,
    .appendExmo = &stdAppendExmo,
    .scaleMod   = &stdScaleMod,
    .shift      = &stdShift
};

void *PLcreateStdCopy(polyType *type, void *poly) {
    return PLcreateCopy(stdpoly,type,poly);
}

void *PLcreateCopy(polyType *newtype, polyType *type, void *poly) {
    stp *res;
    if (newtype == type) 
        return (newtype->createCopy)(poly);
    if (NULL == (res = (newtype->createCopy)(NULL)))
        return NULL;
    if (SUCCESS != PLappendPoly(newtype,res,type,poly,NULL,0,1,0)) {
        (newtype->free)(res); return NULL;
    }
    return res;
}

int PLcompare(polyType *tp1, void *pol1, polyType *tp2, void *pol2, int *res) {
    void *st1, *st2;
    int rcode;
    if ((tp1 == tp2) && (NULL != tp1->compare)) 
        return (tp1->compare)(pol1,pol2,res);
    /* convert both to stdpoly */
    if (stdpoly == tp1) st1 = pol1;
    else st1 = PLcreateCopy(stdpoly,tp1,pol1);
    if (stdpoly == tp2) st2 = pol2;
    else st2 = PLcreateCopy(stdpoly,tp2,pol2);
    rcode = (stdpoly->compare)(st1,st2,res);
    if (st1 != pol1) PLfree(stdpoly,st1);
    if (st2 != pol2) PLfree(stdpoly,st2);
    return rcode;
}

int PLposMultiply(polyType **rtp, void **res,
                  polyType *fftp, void *ff,
                  polyType *sftp, void *sf, int mod) {
    exmo aux; int i, len, rc;
    *rtp = stdpoly; 
    *res = stdCreateCopy(NULL);
    len = PLgetLength(sftp,sf);
    for (i=0;i<len;i++) {
        if (SUCCESS != (rc = PLgetExmo(sftp,sf,&aux,i))) {
            PLfree(*rtp,*res); return rc;
        }
        if (SUCCESS != (rc = PLappendPoly(*rtp,*res,
                                          fftp,ff,
                                          &aux,ADJUSTSIGNS,
                                          aux.coeff,mod))) {
            PLfree(*rtp,*res); return rc;
        }
    }
    PLcancel(*rtp,*res,mod);
    return SUCCESS;  
}

int PLnegMultiply(polyType **rtp, void **res,
                  polyType *fftp, void *ff,
                  polyType *sftp, void *sf, int mod) {

    
    return FAILIMPOSSIBLE;
}

int PLsteenrodMultiply(polyType **rtp, void **res,
                       polyType *fftp, void *ff,
                       polyType *sftp, void *sf, primeInfo *pi) {
    
    return FAILIMPOSSIBLE;
}
