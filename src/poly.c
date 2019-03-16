/*
 * Monomials, polynomials, and basic operations
 *
 * Copyright (C) 2004-2019 Christian Nassau <nassau@nullhomotopie.de>
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

#define LOGPL(func) if (0) printf(#func)
#define LOGPLFMT(func,fmt,dat) if (0) printf(#func ": " fmt "\n", dat)

/**** extended monomials ***********************************************************/

int exmoGetRedLen(exmo *e) {
    int pad = exmoGetPad(e), j;
    for (j=NALG;j--;)
        if (e->r.dat[j] != pad)
            return (j+1);
    return 0;
}

int exmoGetLen(exmo *e) {
    int pad = exmoGetPad(e), res, ex = e->ext, j;
    /* check if ext & pad have the same sign */
    if ((ex<0) && (pad>=0)) return NALG;
    if ((ex>=0) && (pad<0)) return NALG;
    for (res=0, ex=e->ext; pad != ex; res++) ex >>= 1;
    for (j=res;j<NALG;j++)
        if (e->r.dat[j] != pad)
            res = j+1;
    return res;
}

int exmoGetPad(exmo *e) {
    return (e->r.dat[NALG-1] < 0) ? -1 : 0;
}

void copyExmo(exmo *dest, const exmo *src) {
    memcpy(dest,src,sizeof(exmo));
}

void clearExmo(exmo *e) {
    memset(e,0,sizeof(exmo));
    e->coeff = 1;
}

void shiftExmo2(exmo *e, const exmo *s, int scale, int flags) {
    if (0 != (flags & USENEGSHIFT)) {
        int ee = -1 - e->ext, se = -1 - s->ext;
        if ((scale>1) && (se!=0)) {
            e->coeff = 0; return;  /* square of exterior part is zero */
        }
#ifdef USESSE2
        {
            const __m128i one = _mm_set1_epi16(1);
            __m128i aux = _mm_add_epi16(e->r.ssedat,s->r.ssedat);
            e->r.ssedat = _mm_add_epi16(aux,one);
        }
#else
        {
            int i;
            for (i=NALG;i--;)
                e->r.dat[i] += s->r.dat[i] + 1;
        }
#endif
        if (0 != (flags & ADJUSTSIGNS)) {
            if (0 != (ee & se)) e->coeff = 0;
            if (0 != (1 & SIGNFUNC(ee,se))) e->coeff = - e->coeff;
        }
        e->ext = -1 - (ee ^ se);
    } else {
        if ((scale>1) && (s->ext!=0)) {
            e->coeff = 0; return;  /* square of exterior part is zero */
        }
#ifdef USESSE2
        {
            __m128i aux = _mm_mullo_epi16(s->r.ssedat,_mm_set1_epi16(scale));
            e->r.ssedat = _mm_add_epi16(e->r.ssedat,aux);
        }
#else
        {
            int i;
            for (i=NALG;i--;)
                e->r.dat[i] += s->r.dat[i] * scale;
        }
#endif
        if (0 != (flags & ADJUSTSIGNS)) {
            if (0 != (e->ext & s->ext)) e->coeff = 0;
            if (0 != (1 & SIGNFUNC(e->ext,s->ext))) e->coeff = - e->coeff;
        }
        e->ext ^= s->ext;
    }
}

void shiftExmo(exmo *e, const exmo *s, int flags) { shiftExmo2(e,s,1,flags); }

void reflectExmo(exmo *e) {
    int i;
    for (i=NALG;i--;) e->r.dat[i] = -1 - e->r.dat[i];
    e->ext = -1 - e->ext;
}

void negateExmo(exmo *e) {
    int i;
    for (i=NALG;i--;) e->r.dat[i] = - e->r.dat[i];
}

#define COMPRET(x,y) if (0 != (diff = ((x)-(y)))) return diff;
int compareExmo(const void *aa, const void *bb) {
    int diff, i;
    const exmo *a = (const exmo *) aa;
    const exmo *b = (const exmo *) bb;
    COMPRET(a->gen,b->gen);
    COMPRET(a->ext,b->ext);
    for (i=0;i<NALG;i++)
        COMPRET(a->r.dat[i],b->r.dat[i]);
    return 0;
}

int isposExmo(const exmo *e) {
    int i;
    if (e->ext < 0) return 0;
    for (i=0;i<NALG;i++)
        if (e->r.dat[i] < 0)
            return 0;
    return 1;
}

int isnegExmo(const exmo *e) {
    int i;
    if (e->ext >= 0) return 0;
    for (i=0;i<NALG;i++)
        if (e->r.dat[i] >= 0)
            return 0;
    return 1;
}

void exmoSetMaxAlg(primeInfo *pi, exmo *dst) {
    int i;
    dst->ext = -1;
    for (i=0;i<NALG;i++) dst->r.dat[i] = pi->maxpowerXint;
}

void exmoSetMinAlg(primeInfo *pi, exmo *dst) {
    int i;
    dst->ext = 0;
    for (i=0;i<NALG;i++) dst->r.dat[i] = 1;
}

void motateExmo(exmo *e) {
  int edat=e->ext, i;
  for(i=0;i<NALG;i++,(edat>>=1)) e->r.dat[i] = (e->r.dat[i]<<1) | (edat&1);
  e->ext=0;
}

void etatomExmo(exmo *e) {
  int edat=0,emsk=1,i;
  for(i=0;i<NALG;i++,(emsk<<=1)) {
    int r = e->r.dat[i];
    if(r&1) edat |= emsk;
    e->r.dat[i] >>= 1;
  }
  e->ext = edat;
}

void copyExpExmo(primeInfo *pi, exmo *dst, const exmo *src) {
    int i;
    dst->ext = src->ext;
    for (i=0;i<NALG;i++) {
        int log = src->r.dat[i];
        dst->r.dat[i] = (log < pi->maxpowerXintI) ?
            ((log>=0) ? pi->primpows[src->r.dat[i]] : 1) :  pi->maxpowerXint;
    }
}

int exmoRdeg(primeInfo *pi, const exmo *ex) {
    int res, i;
    for (res=i=0; i<NALG; i++) {
        res += ex->r.dat[i] * pi->reddegs[i];
    }
    return res;
}

int exmoIdeg(primeInfo *pi, const exmo *ex) {
    int res;
    res = exmoRdeg(pi, ex);
    res *= pi->tpmo;
    return res + extdeg(pi, ex->ext);
}

int exmoIsAbove(const exmo *a, const exmo *b) {
    int i;
    if (b->ext != (b->ext & a->ext)) return 0;
    for (i=0;i<NALG;i++)
        if (a->r.dat[i] < b->r.dat[i])
            return 0;
    return 1;
}

int exmoIsBelow(const exmo *a, const exmo *b) {
    int i;
    if (a->ext != (b->ext & a->ext)) return 0;
    for (i=0;i<NALG;i++)
        if (a->r.dat[i] > b->r.dat[i])
            return 0;
    return 1;
}

/**** generic polynomials *********************************************************/

#define CALLIFNONZERO1(func,arg1) \
if (NULL != (func)) (func)(arg1);

int PLgetInfo(polyType *type, void *poly, polyInfo *res) {
    if (NULL != type->getInfo) {
        int rcode = (type->getInfo)(poly, res);
        if (SUCCESS != rcode) return rcode;
        res->maxRedLength = PLgetMaxRedLength(type, poly);
        return SUCCESS;
    }
    return FAILIMPOSSIBLE;
}

int PLgetNumsum(polyType *type, void *poly) {
    return (type->getNumsum)(poly);
}

int PLgetMaxRedLengthMotivic(polyType *type, void *poly) {
  return NALG; // FIXME
}

int PLgetMaxRedLength(polyType *type, void *poly) {
    int rval, i, j, num;
    if (NULL != type->getMaxRedLength)
        if (SUCCESS == (type->getMaxRedLength)(poly, &rval))
            return rval;
    num = PLgetNumsum(type, poly);
    if (NULL != type->getExmoPtr) {
        exmo *aux;
        for (rval=i=0;i<num;i++) {
            if (SUCCESS != (type->getExmoPtr)(poly, &aux, i))
                goto exit;
            if ((j = exmoGetRedLen(aux)) > rval) rval = j;
        }
        return rval;
    }

    /* here we could try the same with getExmo, but not right now! */

 exit:
    return NALG; /* only appropriate for stdpoly (?) */
}

void PLfree(polyType *type, void *poly) {
    LOGPLFMT(PLfree,"poly=%p",poly);
    CALLIFNONZERO1(type->free,poly);
}

int PLclear(polyType *type, void *poly) {
    if (NULL == type->clear) return FAILIMPOSSIBLE;
    (type->clear)(poly);
    return SUCCESS;
}

void *PLcreate(polyType *type) {
    void *res = NULL;
    if (NULL != type->createCopy)
        res = (type->createCopy)(NULL);
    LOGPLFMT(PLcreate,"res=%p",res);
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

int PLappendExmo(polyType *dtp, void *dst, const exmo *e) {
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
    len = (stp->getNumsum)(src);
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

int PLcollectCoeffs(polyType *type, void *self, const exmo *ex,
                    int *rval, int mod, int flags) {
    if (NULL != type->collectCoeffs)
        return (type->collectCoeffs)(self, ex, rval, mod, flags);

    ASSERT(NULL == "collectCoeff not fully implemented");
    return SUCCESS;
}

/**** standard polynomial type ****************************************************/

int stdGetInfo(void *src, polyInfo *pli) {
    stp *s = (stp *) src;
    pli->name = "expanded";
    pli->bytesAllocated = sizeof(stp) + (s->nalloc * sizeof(exmo));
    pli->bytesUsed      = sizeof(stp) + (s->num * sizeof(exmo));
    return SUCCESS;
}

void *stdCreateCopy(void *src) {
    stp *s = (stp *) src, *n;
    LOGSTD("CreateCopy");
    if (NULL == (n = (stp *) mallox(sizeof(stp)))) return NULL;
    if (NULL == s) {
        n->num = n->nalloc = 0; n->dat = NULL;
        return n;
    }
    n->nalloc = n->num = s->num;
    if (0 == n->nalloc) n->nalloc = 1;
    if (NULL == (n->dat = (exmo *) mallox(sizeof(exmo) * n->nalloc))) {
        freex(n); return NULL;
    }
    if(s->dat) memcpy(n->dat,s->dat,sizeof(exmo) * s->num);
    return n;
}

void stdFree(void *self) {
    stp *s = (stp *) self;
    LOGSTD("Free");
    /* printf("nalloc = 0x%x, num = 0x%x, dat = %p\n",s->nalloc,s->num,s->dat); */
    if (NULL != s->dat) freex(s->dat);
    s->nalloc = s->num = 0; s->dat = NULL;
    freex(s);
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
    if (0 == nalloc) nalloc = 1;
    if (NULL == (ndat = reallox(s->dat,sizeof(exmo) * nalloc)))
        return FAILMEM;
    s->dat = ndat; s->nalloc = nalloc;
    return SUCCESS;
}

void stdSort(void *self) {
    stp *s = (stp *) self;
    LOGSTD("Sort");
    if(s->dat) qsort(s->dat,s->num,sizeof(exmo),compareExmo);
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

int stdLookup(void *self, const exmo *ex, int *coeff) {
    stp *s = (stp *) self;
    exmo *aux = bsearch(ex, s->dat, s->num, sizeof(exmo), compareExmo);
    if (NULL == aux) return -1;
    if (NULL != coeff) *coeff = aux->coeff;
    return aux - s->dat;
}

int stdRemove(void *self, int idx) {
    stp *s = (stp *) self;
    if ((idx<0) || (idx>s->num)) return FAILIMPOSSIBLE;
    memmove(&(s->dat[idx]), &(s->dat[idx+1]), sizeof(exmo) * (s->num - idx));
    s->num--;
    return SUCCESS;
}

int stdCompare(void *pol1, void *pol2, int *res, int flags) {
    stp *s1 = (stp *) pol1;
    stp *s2 = (stp *) pol2;
    int i;
    LOGSTD("Compare");
    if (0 == (flags & PLF_ALLOWMODIFY))
        return FAILIMPOSSIBLE;
    stdCancel(pol1,0);
    stdCancel(pol2,0);
    if (s1->num != s2->num) {
        *res = s1->num - s2->num;
        return SUCCESS;
    }
    /* we cannot just use memcmp here because there might be undefined
     * bytes at the end of our exmo structures, due to alignment issues */
    for (i=0;i<s1->num;i++) {
        int diff = compareExmo(&(s1->dat[i]),&(s2->dat[i]));
        if (diff) {
            *res = diff;
            return SUCCESS;
        }
        diff = s1->dat[i].coeff - s2->dat[i].coeff;
        if (diff) {
            *res = diff;
            return SUCCESS;
        }
    }
    *res = 0;
    return SUCCESS;
}

void stdReflect(void *self) {
    stp *s = (stp *) self;
    int i;
    LOGSTD("Reflect");
    for (i=0;i<s->num;i++)
        reflectExmo(&(s->dat[i]));
}

void stdMotate(void *self) {
    stp *s = (stp *) self;
    int i;
    LOGSTD("Motate");
    for (i=0;i<s->num;i++)
        motateExmo(&(s->dat[i]));
}

void stdEtatom(void *self) {
    stp *s = (stp *) self;
    int i;
    LOGSTD("Motate");
    for (i=0;i<s->num;i++)
        etatomExmo(&(s->dat[i]));
}

void stdShift(void *self, const exmo *ex, int flags) {
    stp *s = (stp *) self;
    int i;
    LOGSTD("Shift");
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

int stdAppendExmo(void *self, const exmo *ex) {
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
    if (0 == scale) {
        stdClear(self);
        return SUCCESS;
    }
    for (i=0;i<s->num;i++) {
        exmo *e = &(s->dat[i]);
        e->coeff *= scale; if (modulo) e->coeff %= modulo;
    }
    return SUCCESS;
}

int stdCollectCoeffs(void *self, const exmo *e, int *coeff, int mod, int flags) {
    stp *s = (stp *) self;
    const exmo *w,*b,*t;
    if (0 == (flags & PLF_ALLOWMODIFY))
        return FAILIMPOSSIBLE;
    stdSort(self);
    *coeff = 0;
    if (NULL == (w = bsearch(e,s->dat,s->num,sizeof(exmo),compareExmo)))
        return SUCCESS;
    for (b=w-1;b>=s->dat;b--)
        if (0 != compareExmo(b,e))
            break;
    for (t=w+1;t<(s->dat + s->num);t++)
        if (0 != compareExmo(t,e))
            break;
    for (w=b+1;w<t;w++) {
        *coeff += w->coeff;
        if (mod) *coeff %= mod;
    }
    return SUCCESS;
}

/* GCC extension alert! (Designated Initializers)
 *
 * see http://gcc.gnu.org/onlinedocs/gcc-3.2.3/gcc/Designated-Inits.html */

struct polyType stdPolyType = {
    .getInfo    = &stdGetInfo,
    .createCopy = &stdCreateCopy,
    .free       = &stdFree,
    .swallow    = &stdSwallow,
    .clear      = &stdClear,
    .cancel     = &stdCancel,
    .compare    = &stdCompare,
    .reflect    = &stdReflect,
    .motate     = &stdMotate,
    .etatom     = &stdEtatom,
    .getExmoPtr = &stdGetExmoPtr,
    .getNumsum  = &stdGetLength,
    .appendExmo = &stdAppendExmo,
    .scaleMod   = &stdScaleMod,
    .shift      = &stdShift,
    .lookup     = &stdLookup,
    .remove     = &stdRemove,
    .collectCoeffs = &stdCollectCoeffs
};

void *PLcreateStdCopy(polyType *type, void *poly) {
    return PLcreateCopy(stdpoly,type,poly);
}

void *PLcreateCopy(polyType *newtype, polyType *type, void *poly) {
    stp *res;
    LOGPLFMT(PLcreateCopy, "orig at %p",poly);
    if (newtype == type)
        return (newtype->createCopy)(poly);
    if (NULL == (res = (newtype->createCopy)(NULL)))
        return NULL;
    if (SUCCESS != PLappendPoly(newtype,res,type,poly,NULL,0,1,0)) {
        (newtype->free)(res); return NULL;
    }
    return res;
}

int PLcompare(polyType *tp1, void *pol1, polyType *tp2,
              void *pol2, int *res, int flags) {
    void *st1, *st2;
    int rcode;
    if ((tp1 == tp2) && (NULL != tp1->compare))
        return (tp1->compare)(pol1, pol2, res, flags);
    /* create private stdpoly copies */
    st1 = PLcreateCopy(stdpoly,tp1,pol1);
    st2 = PLcreateCopy(stdpoly,tp2,pol2);
    rcode = (stdpoly->compare)(st1, st2, res, PLF_ALLOWMODIFY);
    PLfree(stdpoly,st1);
    PLfree(stdpoly,st2);
    return rcode;
}

int PLtest(polyType *tp, void *pol1, pprop prop) {
    int i, len; exmo e;
    if (NULL != tp->test)
        return (tp->test)(pol1, prop);
    len = PLgetNumsum(tp,pol1);
    switch (prop) {
        case ISPOSITIVE:
            for (i=0;i<len;i++)
                if (SUCCESS != PLgetExmo(tp,pol1,&e,i))
                    return FAILIMPOSSIBLE;
                else
                    if (!isposExmo(&e)) return FAILUNTRUE;
            return SUCCESS;
        case ISNEGATIVE:
            for (i=0;i<len;i++)
                if (SUCCESS != PLgetExmo(tp,pol1,&e,i))
                    return FAILIMPOSSIBLE;
                else
                    if (!isnegExmo(&e)) return FAILUNTRUE;
            return SUCCESS;
        case ISPOSNEG:
            for (i=0;i<len;i++)
                if (SUCCESS != PLgetExmo(tp,pol1,&e,i))
                    return FAILIMPOSSIBLE;
                else
                    if (!(isposExmo(&e) || isnegExmo(&e))) return FAILUNTRUE;
            return SUCCESS;
    }
    return FAILIMPOSSIBLE;
}

int PLposMultiply(polyType **rtp, void **res,
                  polyType *fftp, void *ff,
                  polyType *sftp, void *sf, int mod) {
    exmo aux; int i, len, rc;
    *rtp = stdpoly;
    *res = stdCreateCopy(NULL);
    len = PLgetNumsum(sftp,sf);
    for (i=0;i<len;i++) {
        if (SUCCESS != (rc = PLgetExmo(sftp,sf,&aux,i))) {
            PLfree(*rtp,*res);
            return rc;
        }
        if (SUCCESS != (rc = PLappendPoly(*rtp,*res,
                                          fftp,ff,
                                          &aux,ADJUSTSIGNS,
                                          aux.coeff,mod))) {
            PLfree(*rtp,*res);
            return rc;
        }
    }
    PLcancel(*rtp,*res,mod);
    return SUCCESS;
}

int PLnegMultiply(polyType **rtp, void **res,
                  polyType *fftp, void *ff,
                  polyType *sftp, void *sf, int mod) {
    exmo aux; int i, len, rc;
    *rtp = stdpoly;
    *res = stdCreateCopy(NULL);
    len = PLgetNumsum(sftp,sf);
    for (i=0;i<len;i++) {
        if (SUCCESS != (rc = PLgetExmo(sftp,sf,&aux,i))) {
            PLfree(*rtp,*res);
            return rc;
        }

        if (SUCCESS != (rc = PLappendPoly(*rtp,*res,
                                          fftp,ff,
                                          &aux,ADJUSTSIGNS | USENEGSHIFT,
                                          aux.coeff,mod))) {
            PLfree(*rtp,*res);
            return rc;
        }
    }

    PLcancel(*rtp,*res,mod);
    return SUCCESS;
}

#include "mult.h"

int PLsteenrodMultiply(polyType **rtp, void **res,
                       polyType *fftp, void *ff,
                       polyType *sftp, void *sf,
                       primeInfo *pi, const exmo *pro) {
    int flen, slen;
    int fpos = (SUCCESS == PLtest(fftp,ff,ISPOSITIVE));
    int fneg = (SUCCESS == PLtest(fftp,ff,ISNEGATIVE));
    int spos = (SUCCESS == PLtest(sftp,sf,ISPOSITIVE));
    int sneg = (SUCCESS == PLtest(sftp,sf,ISNEGATIVE));

    /* check if both factors are all positive or all negative */
    if ((!(fpos || fneg)) || (!(spos || sneg)))
        return FAILIMPOSSIBLE;

    /* check if both factors are non-zero */
    if ((0 == (flen = PLgetNumsum(fftp,ff)))
        || (0 == (slen = PLgetNumsum(sftp,sf)))) {
        *rtp = stdpoly;
        *res = PLcreate(*rtp);
        return SUCCESS;
    }

    /* negative times negative is undefined */
    if (fneg && sneg) return FAILIMPOSSIBLE;

    *rtp = stdpoly;
    *res = PLcreate(*rtp);
    stdAddProductToPoly(*rtp, *res, fftp, ff, sftp, sf, pi, pro, fpos, spos);

    PLcancel(*rtp, *res, pi->prime);

    return SUCCESS;
}

int PLEBPMultiply(polyType **rtp, void **res,
		  polyType *fftp, void *ff,
		  polyType *sftp, void *sf,
		  primeInfo *pi) {
    int fpos = (SUCCESS == PLtest(fftp,ff,ISPOSITIVE));
    int spos = (SUCCESS == PLtest(sftp,sf,ISPOSITIVE));

    if (!fpos || !spos) {
	return FAILIMPOSSIBLE;
    }

    *rtp = stdpoly;
    *res = PLcreate(*rtp);
    stdAddProductToPolyEBP(*rtp, *res, fftp, ff, sftp, sf, pi);

    PLcancel(*rtp, *res, pi->prime2);

    return SUCCESS;
}

typedef struct {
    exmo *ex;
    void *val;
} sortitem;

int compSortItem(const void *a, const void *b) {
    const sortitem *aa = (const sortitem *) a;
    const sortitem *bb = (const sortitem *) b;
    return compareExmo(aa->ex, bb->ex);
}

#define SLOG 0

int stdpolySortWithValues(void *poly, void **values) {
    stp *s = (stp *) poly;
    sortitem *sit;
    int i, snum;

    if (NULL == (sit = mallox(sizeof(sortitem) * (s->num + 0))))
        return FAILMEM;

    if (SLOG) printf("Sorting:");

    snum = s->num;

    for (i=0;i<s->num;i++) {
        sit[i].ex  = &(s->dat[i]);
        sit[i].val = values[i];
        if (SLOG) printf(" %p", values[i]);
    }

    qsort(sit, s->num, sizeof(sortitem), compSortItem);
    stdSort(s);

    if (SLOG) ASSERT(snum == s->num);

    if (SLOG) printf("\nResult:");

    for (i=0;i<s->num;i++) {
        values[i] = sit[i].val;
        if (SLOG) printf(" %p", values[i]);
    }

    freex(sit);

    if (SLOG) printf("\n");

    return SUCCESS;
}
