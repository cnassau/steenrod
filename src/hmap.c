/*
 * Hopf maps - used to implement Hopf algebroid structure maps
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

#include <tcl.h>
#include <string.h>
#include "tprime.h"
#include "tpoly.h"
#include "common.h"
#include "hmap.h"

#define XVAL 0xffff

#define RETERR(errmsg) \
{ if (NULL != ip) Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; }

#define BINT Tcl_WideInt

static struct {
    int max;
    BINT *data;
} binomTable;

int makeBinomTable(int max) {
    int size = (max+2)*(max+1) >> 1, i,j;
    BINT *new, *cur, *prv;

    if (max <= binomTable.max)
        return TCL_OK;

    if (NULL == (new = mallox(sizeof(BINT) * size))) 
        return TCL_ERROR;
    new[0] = 1;
    for (i=1;i<=max;i++) {
        prv = new + ((i*(i-1))>>1);
        cur = new + ((i*(i+1))>>1);
        for (j=1;j<i;j++) 
            cur[j] = prv[j] + prv[j-1];
        cur[0] = cur[i] = 1;
    }
    if (NULL != binomTable.data) 
        freex(binomTable.data);
    binomTable.data = new;
    binomTable.max = max;
    return TCL_OK;
}

BINT binom(int sum, int j) {
    if (sum >= binomTable.max)
        if (TCL_OK != makeBinomTable(sum+20))
            return -1;
    return binomTable.data[(sum*(sum+1)>>1) + j];
}

int Tcl_MultinomialCmd(ClientData cd, Tcl_Interp *ip,
                       int objc, Tcl_Obj * const objv[]) {
    
    int sum=0, aux, i;
    BINT res = 1;

    for (i=1;i<objc;i++) {
        if (TCL_OK != Tcl_GetIntFromObj(ip,objv[i],&aux))
            return TCL_ERROR;
        if (aux < 0) 
            RETERR("negative entries not allowed");

        sum += aux;
        if (TCL_OK != makeBinomTable(sum))
            RETERR("Out of memory");

        res *= binom(sum,aux);
    }

    Tcl_SetObjResult(ip, Tcl_NewWideIntObj(res));
    return TCL_OK;
}

/* An hmap contains the description of a multiplicative map from the
 * extended polynomial algebra to a tensor power of itself. 
 *
 * The tensor factors in the source and destination are called "slots".
 * There is one slot for the source (S0) and possibly many slots for 
 * the destination; these come in two types:
 *
 *    monomial slots: M0, M1, ...
 *    integer slots:  I0, I1, ...    (used for all sorts of degrees)
 *     
 * Slot references use a new Tcl_Obj type tclHMS. */

typedef enum { HMS_MONO, HMS_INT, HMS_SOURCE } slottype;

static Tcl_ObjType tclHMS;

#define PTR1(objptr) ((objptr)->internalRep.twoPtrValue.ptr1)
#define PTR2(objptr) ((objptr)->internalRep.twoPtrValue.ptr2)

#define SLOTTYPE(objPtr) ((slottype) PTR1(objPtr)) 
#define SLOTNUM(objPtr) ((int) PTR2(objPtr))

#define SLOTCONVERT(ip,objPtr) Tcl_ConvertToType(ip,objPtr,&tclHMS)

int HMSSetFromAnyProc(Tcl_Interp *ip, Tcl_Obj *objPtr) {
    int num, len;
    char *aux = Tcl_GetStringFromObj(objPtr, &len);
    slottype st;

    if (0 == len) return TCL_ERROR;

    switch (*aux) {
        case 'S':
            st = HMS_SOURCE; break;
        case 'M':
            st = HMS_MONO; break;
        case 'I':
            st = HMS_INT; break;
        default:
            return TCL_ERROR;
    }

    if (1 != sscanf(aux+1,"%d",&num)) return TCL_ERROR;

    if (num < 0) return TCL_ERROR;

    PTR1(objPtr) = (void *) st;
    PTR2(objPtr) = (void *) num;
    objPtr->typePtr = &tclHMS;

    return TCL_ERROR;
}

void HMSDupInternalRepProc(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr) {
    PTR1(dupPtr) = PTR1(srcPtr);
    PTR2(dupPtr) = PTR2(srcPtr);
    dupPtr->typePtr = &tclHMS;
}

/* Structure maps are stored as a collection of summands. The summands 
 * support extra fields that can hold the results of partial evaluations of
 * the structure map. */

typedef struct {
    int coeff;
    exmo *edat;
    int  *idat;
} hmap_tensor;

void hmapTclTensor(hmap_tensor *h, 
                   Tcl_Obj **a, Tcl_Obj **b, 
                   int numMono, int numInt);

void printTensor(hmap_tensor *h, int nm, int ni) {
    Tcl_Obj *a,*b;
    hmapTclTensor(h,&a,&b,nm,ni);
    printf("<%d * %s : %s>",h->coeff, Tcl_GetString(a), Tcl_GetString(b));
    DECREFCNT(a);
    DECREFCNT(b);
}

void freeTensor(hmap_tensor *h) {
    if (NULL != h->edat)
        freex(h->edat);
    if (NULL != h->idat)
        freex(h->idat);
    h->edat = NULL; h->idat = NULL;
}

int allocTensor(hmap_tensor *h, int numMono, int numInt) {
    h->edat = mallox(sizeof(exmo) * numMono);
    h->idat = mallox(sizeof(int) * numInt);
    if ((NULL == h->edat) || (NULL == h->idat)) {
        freeTensor(h);
        return FAILMEM;
    }
    return SUCCESS;
} 

void clearTensor(hmap_tensor *h, int numMono, int numInt) {
    int i;
    h->coeff = 1;
    for (i=0;i<numMono;i++) 
        clearExmo(&(h->edat[i]));
    for (i=0;i<numInt;i++) 
        h->idat[i] = 0;
}

void copyTensor(hmap_tensor *dst, hmap_tensor *src, int numMono, int numInt) {
    int i;
    dst->coeff = src->coeff;
    for (i=0;i<numMono;i++) 
        copyExmo(&(dst->edat[i]),&(src->edat[i]));
    for (i=0;i<numInt;i++) 
        dst->idat[i] = src->idat[i];
}

#define LOG(x) if (0) { x };

void multTensor(hmap_tensor *dst, hmap_tensor *src, int scale, int numMono, int numInt, int modval) {
    int i,j,sign=0;
    int ddeg=0, sdeg=0;
    LOG(printTensor(dst,numMono,numInt); printf(" *= "); printTensor(src,numMono,numInt););
    //dst->coeff = 1;
    for (i=0;i<numMono;i++) {
        ddeg += BITCOUNT(dst->edat[i].ext);
        sdeg += BITCOUNT(src->edat[i].ext);
        if (1 & sign) 
            if (1 & BITCOUNT(dst->edat[i].ext))
                dst->coeff = - dst->coeff;
        shiftExmo2(&(dst->edat[i]),&(src->edat[i]),scale,ADJUSTSIGNS);
        dst->coeff *= dst->edat[i].coeff;
        dst->edat[i].coeff = 1;
        for (j=scale;j--;) {
            dst->coeff *= src->coeff; 
            if (modval) dst->coeff %= modval;
        }
        sign += BITCOUNT(src->edat[i].ext);
    }
    // if (0 != (1 & sdeg & ddeg)) dst->coeff = - dst->coeff;
    for (i=0;i<numInt;i++) {
        dst->idat[i] += src->idat[i] * scale;
    }    
    LOG(printf(" = "); printTensor(dst,numMono,numInt); printf("\n"););
}

int compareTensor(hmap_tensor *a, hmap_tensor *b, hmap_tensor *res, int numMono, int numInt) {
    int i;
    for (i=0;i<numMono;i++)
        res->edat[i].coeff = exmoIsBelow(&(a->edat[i]),&(b->edat[i]));
    for (i=0;i<numInt;i++) {
        res->idat[i] = a->idat[i] - b->idat[i];
    }
    return 1;
}

typedef enum { EXT, RED } gentype;

typedef struct {

    /* generator that has this summand in its image */
    gentype gtype; 
    int     idx; 

    int value;  /* the value that we've chosen */ 
    int goodbits; /* limit on value */
    int quant;  /* quantization of value */

    hmap_tensor sumdat; /* the actual summand */

    exmo source;        /* partial product */
    hmap_tensor pardat; /* partial product */

} hmap_summand;

void freeSummand(hmap_summand *h) {
    freeTensor(&(h->sumdat));
    freeTensor(&(h->pardat));
}

hmap_summand *allocSummand(int numMono, int numInt) {
    hmap_summand *h = callox(1, sizeof(hmap_summand));

    if (NULL == h) return NULL;

    h->goodbits = XVAL;
    h->quant  = 1;
    
    if ((SUCCESS != allocTensor(&(h->sumdat),numMono,numInt)) ||
        (SUCCESS != allocTensor(&(h->pardat),numMono,numInt))) {
        freeSummand(h);
        freex(h);
        return NULL;
    }
    
    return h;
}

int hmapInitSummand(hmap_summand *h, Tcl_Interp *ip, 
                    Tcl_Obj *mlist, Tcl_Obj *ilist, 
                    int numMono, int numInt) {
    int i, aux, cnt;
    Tcl_Obj **lst;

    if (TCL_OK != Tcl_ListObjGetElements(ip, mlist, &cnt, &lst))
        return TCL_ERROR;
    
    if (cnt != numMono) 
        RETERR("signature mismatch: wrong number of monomials");
    
    h->sumdat.coeff = 1;
    
    for (i=0;i<cnt;i++) {
        if (TCL_OK != Tcl_ConvertToExmo(ip, lst[i])) 
            return TCL_ERROR;
        copyExmo(&(h->sumdat.edat[i]),exmoFromTclObj(lst[i]));    
    }

    if (TCL_OK != Tcl_ListObjGetElements(ip, ilist, &cnt, &lst))
        return TCL_ERROR;

    if (cnt != numInt) 
        RETERR("signature mismatch: wrong number of integers");
     
    for (i=0;i<cnt;i++) {
        if (TCL_OK != Tcl_GetIntFromObj(ip, lst[i], &aux)) 
            return TCL_ERROR;
        h->sumdat.idat[i] = aux;
    }

    return TCL_OK;
}

typedef struct {

    /* signature */
    int numMono;    /* number of monomial slots */ 
    int numInt;     /* number of integer slots */

    /* the summands */
    int numSummands, numAlloc; 
    hmap_summand **summands;

    /* restrictions */
    exmo sourceRestriction; 
    hmap_tensor restrictions1; /* integer slots: 0 - no restriction, 1 - equal, 2 - less/equal */
    hmap_tensor restrictions2; /* integer slots: value of the restriction */ 
    int modval;

    hmap_tensor comparison1; /* result of the last comparison operation */
    int comparison2;

    int error;

    /* callback */
    int (*callback)(void *self);
    void *callbackdata1;
    void *callbackdata2;

} hmap;

void hmapTclTensor(hmap_tensor *h, 
                       Tcl_Obj **a, Tcl_Obj **b, 
                       int numMono, int numInt) {
    Tcl_Obj **lst;
    int i;

    lst = mallox(sizeof(Tcl_Obj *) * numMono);
    if (NULL == lst) {
        *a = Tcl_NewStringObj("Out of memory",13);
    } else {
        for (i=0;i<numMono;i++)
            lst[i] = Tcl_NewExmoCopyObj(&(h->edat[i]));
        *a = Tcl_NewListObj(numMono, lst);
        freex(lst);
    }

    lst = mallox(sizeof(Tcl_Obj *) * numInt);
    if (NULL == lst) {
        *b = Tcl_NewStringObj("Out of memory",13);
    } else {
        for (i=0;i<numInt;i++)
            lst[i] = Tcl_NewIntObj(h->idat[i]);
        *b = Tcl_NewListObj(numInt, lst);
        freex(lst);
    }    
}

#define LogStuff 0
#define LOGMSG(format,arg) { if (LogStuff) {printf("> " format "\n", arg);}; }

int invokeCallback(Tcl_Interp *ip, Tcl_Obj *callback, hmap *hm,  hmap_summand *result) {
    Tcl_Obj *(command[5]);
    int retval;

    LOGMSG("invokeCallback result=%p",result);

    command[0] = callback;
    command[1] = Tcl_NewExmoCopyObj(&(result->source));
    command[2] = Tcl_NewIntObj(result->pardat.coeff);
    hmapTclTensor(&(result->pardat),&(command[3]),&(command[4]),hm->numMono,hm->numInt);

    INCREFCNT(command[1]);
    INCREFCNT(command[2]);
    INCREFCNT(command[3]);
    INCREFCNT(command[4]);

    retval = Tcl_EvalObjv(ip, 5, command, 0);
    if (TCL_OK != retval) hm->error = 1;

    DECREFCNT(command[1]);
    DECREFCNT(command[2]);
    DECREFCNT(command[3]);
    DECREFCNT(command[4]);

    return retval;
}

int hmapGrow(hmap *hm, int newSumnum) {
    hmap_summand **new;
    int i;

    newSumnum++; /* we always need space for one extra summand at the end */

    if (newSumnum <= hm->numAlloc)
        return SUCCESS;
    
    if (newSumnum <= hm->numAlloc+30) 
        newSumnum = hm->numAlloc+30;
    
    if (NULL == (new = reallox(hm->summands, newSumnum * sizeof(hmap_summand *))))
        return FAILMEM;

    for (i=hm->numAlloc;i<newSumnum;i++) {
        if (NULL == (new[i] = allocSummand(hm->numMono, hm->numInt))) 
            return FAILMEM;
    }
    
    hm->summands = new;
    hm->numAlloc = newSumnum;

    return SUCCESS;
}

int stdTclCallback(void *self) {
    hmap *hm = (hmap *) self;
    hmap_summand *result = hm->summands[hm->numSummands];
    Tcl_Interp *ip = (Tcl_Interp *) hm->callbackdata1;

    LOGMSG("stdTclCallback result=%p",result);

    return invokeCallback(ip, (Tcl_Obj *) hm->callbackdata2, hm, result);
}

Tcl_Obj *hmapTclSummand(hmap_summand *h, int numMono, int numInt) {
    Tcl_Obj *sums[8];
    int cnt = 6;
    sums[0] = Tcl_NewStringObj((h->gtype == EXT) ? "E" : "R", 1);
    sums[1] = Tcl_NewIntObj(h->idx);
    sums[2] = Tcl_NewExmoCopyObj(&(h->source));
    sums[3] = Tcl_NewIntObj(h->sumdat.coeff);
    hmapTclTensor(&(h->sumdat),&(sums[4]),&(sums[5]), numMono, numInt);
    if ((h->goodbits != XVAL) || (h->quant != 1)) {
        sums[6] = Tcl_NewIntObj(h->quant);
        cnt++;
    }
    if (h->goodbits != XVAL) {
        sums[7] = Tcl_NewIntObj(h->goodbits);
        cnt++;
    }
    return Tcl_NewListObj(cnt,sums);
}

Tcl_Obj *hmapTclListing(hmap *hm) {
    Tcl_Obj **sums, *res;
    int i;
    
    sums = mallox(sizeof(Tcl_Obj *) * hm->numSummands);

    for (i=0;i<hm->numSummands;i++)
        sums[i] = hmapTclSummand(hm->summands[i],hm->numMono,hm->numInt);

    res = Tcl_NewListObj(hm->numSummands, sums);
    freex(sums);
    return res;
}

int hmapParseSummand(hmap *hm, hmap_summand *nsum,  Tcl_Interp *ip, Tcl_Obj *obj) {
        
    int cnt, aux;
    Tcl_Obj **lst;

    char *usage = "wrong format, expected "
        "'E/R <idx> <source> <coeff> <monomial slots> <integer slots> ?<quant>? ?<goodbits>?'";

#define RETERR2(x) { freeSummand(nsum); RETERR(x); }
#define RETERR3 { freeSummand(nsum); return TCL_ERROR; }

    if (TCL_OK != Tcl_ListObjGetElements(ip, obj, &cnt, &lst))
        return TCL_ERROR;

    if (cnt < 6) RETERR2(usage);

    switch (*(Tcl_GetString(lst[0]))) {
        case 'E': 
            nsum->gtype = EXT; break;
        case 'R': 
            nsum->gtype = RED; break;
        default: 
            RETERR2("Generator type should be E or R");
    }
    
    if (TCL_OK != Tcl_GetIntFromObj(ip, lst[1], &aux)) 
        RETERR3;
    
    if (aux < 0)
        RETERR2("negative index not allowed");

    nsum->idx = aux;

    if (TCL_OK != Tcl_ConvertToExmo(ip, lst[2]))
        RETERR3;

#if 0
    copyExmo(&(nsum->source), exmoFromTclObj(lst[2]));
#endif

    if (TCL_OK != Tcl_GetIntFromObj(ip, lst[3], &aux)) 
        RETERR3;
    
    if (TCL_OK != hmapInitSummand(nsum,ip,lst[4],lst[5],hm->numMono, hm->numInt))
        RETERR3;

    nsum->sumdat.coeff = aux; /* do this after InitSummand */

    if (cnt >= 7) {
        if (TCL_OK != Tcl_GetIntFromObj(ip, lst[6], &aux)) 
            RETERR3;
        nsum->quant = aux;
    }

    if (cnt >= 8) {
        if (TCL_OK != Tcl_GetIntFromObj(ip, lst[7], &aux)) 
            RETERR3;
        nsum->goodbits = aux;
    }

    if (cnt >= 9)
        RETERR2(usage);

    return TCL_OK;    
}

int hmapAddSummand(hmap *hm, Tcl_Interp *ip, Tcl_Obj *obj) {

    hmap_summand *nsum;
    
    if (SUCCESS != hmapGrow(hm, hm->numSummands+1))
        RETERR("Out of memory");

    nsum = hm->summands[hm->numSummands];

    hm->summands[hm->numSummands] = nsum;

    if (TCL_OK != hmapParseSummand(hm, nsum, ip, obj))
        RETERR3;

    hm->numSummands++;
    
    return TCL_OK;
}

int recogOp(Tcl_Obj *obj) {
    char *str = Tcl_GetString(obj);
    switch (*str) {
        case '*': return 0;
        case '=' : return 1;
        case '<' : return 2;
        default: ;
    }
    return -1;
} 

int hmapParseRestrictions(hmap *hm, Tcl_Interp *ip, Tcl_Obj *obj) {
    Tcl_Obj **vec, **vec2;
    int cnt, cnt2, compop, i, j, k;

    if (TCL_OK != Tcl_ListObjGetElements(ip, obj, &cnt, &vec)) 
        return TCL_ERROR;
    
    if (cnt != (1 + hm->numMono + hm->numInt)) 
        RETERR("cannot parse restrictions: wrong number of entries");
    
    for (i=0,j=0;i<=hm->numMono;i++,j++) {
        if (TCL_OK != Tcl_ListObjGetElements(ip, vec[i], &cnt2, &vec2)) 
            return TCL_ERROR;
        
        if ((0 == cnt2) || (cnt2 > 2)) 
            RETERR("restriction problem (monomial slots)");
        
        if (-1 == (compop = recogOp(vec2[0])))
            RETERR("comparison operator not recognized");
        
        if (i) {
            hm->restrictions1.edat[i-1].coeff = compop;
        } else {
            hm->sourceRestriction.coeff = compop;
        }

        if (0 == compop) 
            continue;

        if (cnt2 < 2)
            RETERR("restriction problem (missing whitespace?)");
        
        if (TCL_OK != Tcl_ConvertToExmo(ip,vec2[1]))
            return TCL_ERROR;

        if (i) { 
            copyExmo(&(hm->restrictions1.edat[i-1]), exmoFromTclObj(vec2[1]));
            hm->restrictions1.edat[i-1].coeff = compop;
        } else {
            copyExmo(&(hm->sourceRestriction), exmoFromTclObj(vec2[1]));
            hm->sourceRestriction.coeff = compop;           
        }
    }

    for (i=0;i<hm->numInt;i++,j++) {

        if (TCL_OK != Tcl_ListObjGetElements(ip, vec[j], &cnt2, &vec2)) 
            return TCL_ERROR;
        
        if ((0 == cnt2) || (cnt2 > 2)) 
            RETERR("restriction problem (integer slots)");
        
        if (-1 == (compop = recogOp(vec2[0])))
            RETERR("comparison operator not recognized");
        
        hm->restrictions2.idat[i] = compop;
        
        if (0 == compop) 
            continue;

        if (cnt2 < 2)
            RETERR("restriction problem (missing whitespace?)");        

        if (TCL_OK != Tcl_GetIntFromObj(ip,vec2[1],&k))
            return TCL_ERROR;

        hm->restrictions1.idat[i] = k;
    }

    return TCL_OK;
}

void handleSummand(hmap *hm, int numsum);

int hmapSelectFunc(hmap *hm, Tcl_Interp *ip, Tcl_Obj *rest, Tcl_Obj *callback) {
    
    if (TCL_OK != hmapParseRestrictions(hm, ip, rest))
        return TCL_ERROR;

    hm->error = 0;

    hm->callback = stdTclCallback;
    hm->callbackdata1 = ip;
    hm->callbackdata2 = callback;

    clearTensor(&(hm->summands[0]->pardat),hm->numMono,hm->numInt);
    clearExmo(&(hm->summands[0]->source));

    handleSummand(hm, 0);

    if (hm->error)
        return TCL_ERROR;

    return TCL_OK;
}

void makeNextPartial(hmap *hm, hmap_summand *current, hmap_summand *next);

void pexmo(exmo *e) {
    Tcl_Obj *x = Tcl_NewExmoCopyObj(e);
    printf("%s",Tcl_GetString(x));
    DECREFCNT(x);
}

int checkPartialRestriction(hmap *hm, hmap_summand *next) {
    int i;
    LOGMSG("checkPartialRestriction %p",next);

    if (hm->sourceRestriction.coeff != 0) {
        hm->comparison2 = exmoIsBelow(&(next->source),&(hm->sourceRestriction));
        if (!hm->comparison2) 
            return 0; /* constraint violated */
    }

    compareTensor(&(next->pardat), &(hm->restrictions1), 
                  &(hm->comparison1), hm->numMono, hm->numInt);
    for (i=0;i<hm->numMono;i++) 
        if (hm->restrictions1.edat[i].coeff)
            if (!hm->comparison1.edat[i].coeff)
                return 0; /* constraint violated */
    for (i=0;i<hm->numInt;i++) 
        if (hm->restrictions2.idat[i])
            if (hm->comparison1.idat[i] > 0)
                return 0; /* constraint violated */
    return 1;
}
        
void handleSummand(hmap *hm, int numsum) {

    hmap_summand *current = hm->summands[numsum];
    hmap_summand *next = hm->summands[numsum+1];

    LOGMSG("handleSummand current=%p",current);
    LOGMSG("handleSummand next=%p",next);
    LOGMSG("handleSummand next->pardat=%p",&(next->pardat));

    if (numsum == hm->numSummands) {
        int i;
        
        LOGMSG("numsum == hm->numSummands == %d", numsum);
        
        /* check whether constraints have been *exactly* met */
        if (1 == hm->sourceRestriction.coeff) {
            if (compareExmo(&(current->source),&(hm->sourceRestriction)))
                return;
        }

        for (i=0;i<hm->numMono;i++) {
            if (1 == hm->restrictions1.edat[i].coeff) {
                if (compareExmo(&(hm->restrictions1.edat[i]),&(current->pardat.edat[i])))
                    return; 
            }
        }
        for (i=0;i<hm->numInt;i++) {
            if (1 == hm->restrictions2.idat[i])
                if (hm->comparison1.idat[i])
                    return;
        }

        /* yes => invoke callback */
        hm->callback(hm);
        return;
    }

    for (current->value = 0; 
         current->value == (current->value & current->goodbits); 
         current->value += current->quant) {
        
        makeNextPartial(hm, current, next);

        //invokeCallback((Tcl_Interp *) hm->callbackdata1, (Tcl_Obj *) hm->callbackdata2, hm, next);

        if (!next->pardat.coeff) 
            continue;

        if (!checkPartialRestriction(hm, next)) 
            return; 

        handleSummand(hm, numsum+1);
        
        if (hm->error) return;
    }

}

void makeNextPartial(hmap *hm, hmap_summand *current, hmap_summand *next) {

    if (current->gtype == RED) {
        BINT sum, newcoeff;

        sum = current->source.dat[current->idx] + current->value;

        if (sum != (sum & current->goodbits)) {
            next->pardat.coeff = 0;
            return ;
        }

        newcoeff = current->pardat.coeff * binom(sum, current->value);
        //printf("newcoeff = %d, sum=%d, value=%d\n",newcoeff,sum,current->value);

        if (hm->modval) {
            newcoeff %= hm->modval;
            if (0 == newcoeff) {
                next->pardat.coeff = 0;
                return ;
            } 
        }         

        copyTensor(&(next->pardat),&(current->pardat),hm->numMono,hm->numInt);
        copyExmo(&(next->source),&(current->source));

        next->source.dat[current->idx] = sum;
        next->pardat.coeff = newcoeff;

        multTensor(&(next->pardat),&(current->sumdat),
                   current->value,hm->numMono,hm->numInt,hm->modval);

        next->source.dat[current->idx] = sum;
        next->pardat.coeff = newcoeff;

    } else {

        int sign, val = 1 << current->idx;

        copyTensor(&(next->pardat),&(current->pardat),hm->numMono,hm->numInt);
        copyExmo(&(next->source),&(current->source));

        if (0 == current->value) 
            return;

        if ((current->value>1) || (0 != (next->source.ext & val))) {
            next->pardat.coeff = 0;
            return;
        }

        sign = SIGNFUNC(next->source.ext, val);

        /* TODO: check the signs! the current code is almost surely not correct! */

        multTensor(&(next->pardat),&(current->sumdat),
                   current->value,hm->numMono,hm->numInt,hm->modval);

        if (1 & sign) 
            next->pardat.coeff = -next->pardat.coeff;

        next->source.ext |= val;  
    }

}



hmap *hmapCreate(int numMono, int numInt) {
    hmap *res = mallox(sizeof(hmap));

    if (NULL == res) return NULL;
    
    memset(res, 0, sizeof(hmap));

    res->numMono = numMono;
    res->numInt  = numInt;

    res->numAlloc = res->numSummands = 0;
    res->summands = NULL;

    if (SUCCESS != allocTensor(&(res->restrictions1), numMono, numInt)) {
        freex(res);
        return NULL;
    }

    if (SUCCESS != allocTensor(&(res->restrictions2), numMono, numInt)) {
        freex(res);
        return NULL;
    }

    if (SUCCESS != allocTensor(&(res->comparison1), numMono, numInt)) {
        freex(res);
        return NULL;
    }

    hmapGrow(res, 0); /* make sure there's space for the extra summand */

    return res;
}

void hmapDestroy(hmap *hm) {
    
    if (NULL != hm->summands) {
        int i;
        for (i=0;i<hm->numAlloc;i++) {
            freeSummand(hm->summands[i]);
            freex(hm->summands[i]);
        }
        freeTensor(&(hm->restrictions1));
        freeTensor(&(hm->restrictions2));
        freeTensor(&(hm->comparison1));
        freex(hm->summands);
    }

    freex(hm);
}

/**** TCL INTERFACE **********************************************************/

typedef enum { ADD, LIST, SELECT } hmapcmdcode;

static CONST char *cmdNames[] = { "add", "list", "select", (char *) NULL };

static hmapcmdcode cmdmap[] = { ADD, LIST, SELECT };

int Tcl_HmapWidgetCmd(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * const objv[]) {
    hmap *hm = (hmap *) cd;
    int result, index;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(ip, objv[1], cmdNames, "subcommand", 0, &index);
    if (result != TCL_OK) return result;

    switch (cmdmap[index]) {
        case ADD:
            if (objc != 3) {
                Tcl_WrongNumArgs(ip, 2, objv, "<summand data>");
                return TCL_ERROR;
            }
            return hmapAddSummand(hm,ip,objv[2]);

        case SELECT:
            if (objc != 5) {
                Tcl_WrongNumArgs(ip, 2, objv, "<modval> <restrictions> <callback>");
                return TCL_ERROR;
            }
            
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[2],&(hm->modval)))
                return TCL_ERROR;

            return hmapSelectFunc(hm, ip, objv[3], objv[4]);

        case LIST:
            if (objc != 2) {
                Tcl_WrongNumArgs(ip, 2, objv, NULL);
                return TCL_ERROR;
            }
            
            Tcl_SetObjResult(ip, hmapTclListing(hm));
            return TCL_OK;
    }            

    RETERR("internal error in Tcl_HmapWidgetCmd");
}



void Tcl_DestroyHmap(ClientData cd) {
    hmap *hm = (hmap *) cd;
    hmapDestroy(hm);
}

int Tcl_CreateHmapCmd(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * CONST objv[]) {
    hmap *hm;
    int nm, ni;

    if (objc != 4) {
        Tcl_WrongNumArgs(ip, 1, objv, "name #<mono slots> #<int slots>");
        return TCL_ERROR;
    }

    if (TCL_OK != Tcl_GetIntFromObj(ip,objv[2],&nm))
        return TCL_ERROR;

    if (TCL_OK != Tcl_GetIntFromObj(ip,objv[3],&ni))
        return TCL_ERROR;

    if (NULL == (hm = hmapCreate(nm,ni))) RETERR("out of memory");

    Tcl_CreateObjCommand(ip, Tcl_GetString(objv[1]),
                         Tcl_HmapWidgetCmd, (ClientData) hm, Tcl_DestroyHmap);

    return TCL_OK;
}

int Hmap_HaveTypes = 0;

int Hmap_Init(Tcl_Interp *ip) {

    Tcl_InitStubs(ip, "8.0", 0);
    
    Tptr_Init(ip);
    Tprime_Init(ip);
    Tpoly_Init(ip);

    if (!Hmap_HaveTypes) {
        /* set up types and register */
        tclHMS.name               = "hmap slot reference";
        tclHMS.freeIntRepProc     = NULL;
        tclHMS.dupIntRepProc      = HMSDupInternalRepProc;
        tclHMS.updateStringProc   = NULL;
        tclHMS.setFromAnyProc     = HMSSetFromAnyProc;
        Tcl_RegisterObjType(&tclHMS);

        Hmap_HaveTypes = 1;
    }

    Tcl_CreateObjCommand(ip, POLYNSP "hmap",
                         Tcl_CreateHmapCmd, (ClientData) 0, NULL);

    Tcl_CreateObjCommand(ip, POLYNSP "multinomial",
                         Tcl_MultinomialCmd, (ClientData) 0, NULL);

    return TCL_OK;
}
