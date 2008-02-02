/*
 * Secondary multiplication routine, prime 2
 *
 * Copyright (C) 2004-2008 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "secmult2.h"
#include "tpoly.h"

/* We're implementing a slightly twisted version of (part of) EBP/I^2.
 * For the uninitiated: E stands for the exterior algebra on mu0,mu1,...
 * where "boundary(mu_k) = v_k", and we're about to add support for
 * the expressions
 *
 *    coeff * Sq(R) * v_k * gen   and/or   coeff * Sq(R) * w_k * gen
 *
 * where w_k = 2*mu_k + v_k*mu_0 (mod 4). The v_k and w_k are encoded
 * in the lowest byte of the generator id in our exmo's.
 */

#define HASVW(genid) (0 != ((genid) & 0x30))
#define VWIDX(genid) ((genid) & 0x0f)
#define HASV(genid) (0 != ((genid) & 0x20))
#define HASW(genid) (0 != ((genid) & 0x10))

typedef unsigned char cofft;

typedef struct {
    cofft dat[3+NALG][2+NALG]; /* box entry */
    cofft msk[3+NALG][2+NALG]; /* mask of forbidden bits = diagonal sum */
    cofft rem[3+NALG][2+NALG]; /* vertical remainders */
    cofft sum[3+NALG][2+NALG]; /* vertcial sum */
    cofft cols[3+NALG];        /* bitmask of collisions */
    cofft par[3+NALG];         /* helper field to determine the parity/sign */
    int sign;
    Tcl_Interp *ip;
    polyType *ptp;
    void *pol;
    const exmo *f1;
    exmo *f2;
    int collisionidx, collision;
    int decoration; /* < 0 for wk, > 0 for vk */
} smultmat;

cofft removeBadbits(cofft val, cofft bad) {
    while (0 != (val & bad)) val--;
    return val;
}
cofft removeBadbitsAlmost(cofft val, cofft bad) {
    while (BITCOUNT(val & bad) > 1) val--;
    return val;
}

int SecmultHandleBoxVal(smultmat *mmat, int val,
                        int rownum, int idx, int allowCollisions) {
    unsigned int
        rem = mmat->rem[rownum][idx],
        msk = mmat->msk[rownum][idx],
        collision = mmat->cols[rownum];
    if (idx && (val > rem)) val = rem;
    if (!allowCollisions || (0 != collision)) {
        val = removeBadbits(val,msk);
        /* clear collision field if it referred to this box */
        mmat->cols[rownum] = collision ^ (collision & (1 << idx));
    } else {
        val = removeBadbitsAlmost(val,msk);
        if (0 != (collision = (val & msk))) {
            mmat->collisionidx = idx;
            mmat->collision = collision;
            mmat->cols[rownum] = 1 << idx;
        }
    }
    if (0 != (1 & val)) {
        /* printf("par[%d]:=%d\n",rownum+idx-2,idx); */
        mmat->par[rownum+idx-2] = idx;
    }
    mmat->dat[rownum][idx] = val;
    mmat->msk[rownum-1][idx+1] = msk | val;
    mmat->rem[rownum-1][idx] = rem - val;
    mmat->sum[rownum-1][idx] = mmat->sum[rownum][idx] + val;
    return val;
}

int SecmultFinalRow(smultmat *mmat, int allowCollision) {
    int i, val, nval, ext1=0, ext2=0;
    mmat->sum[1][NALG] = 0;
    for (i=NALG;i--;) {
        val = mmat->f2->r.dat[i] - mmat->sum[1][i+1];
        nval = SecmultHandleBoxVal(mmat,val,1,i+1,allowCollision);
        if( val != nval ) return 0;
        ext1 <<= 1; ext1 |= (1 & val);
        ext2 <<= 1; ext2 |= (1 & mmat->msk[1][i+1]);
    }
    mmat->sign = SIGNFUNC(ext2,ext1);
    printf("sign=%d from %d | %d\n",mmat->sign,ext1,ext2);
    return 1;
}

int SecmultFirstRow(smultmat *mmat, int rownum, int allowCollision) {
    int i, tot, val;
    mmat->cols[rownum] = 0;
    if( 1 == rownum )
        return SecmultFinalRow(mmat,allowCollision);
    tot = mmat->f1->r.dat[rownum-2];
    for (i=NALG;i--;) {
        val = SecmultHandleBoxVal(mmat,tot>>i,rownum,i,allowCollision);
        tot -= val << i;
    }
    return 1;
}

int SecmultNextRow(smultmat *mmat, int rownum, int allowCollision) {
    unsigned int i=1, tot = mmat->dat[rownum][0], val=0, nval;
    if( 1 >= rownum ) return 0;
    do {
        while ((i < NALG) && (0 == (val=mmat->dat[rownum][i]))) i++;
        if (i == NALG) return 0;
        nval = SecmultHandleBoxVal(mmat,val-1,rownum,i,allowCollision);
        tot += (val-nval) << i;
        while (i--) {
            val = SecmultHandleBoxVal(mmat,tot>>i,rownum,i,allowCollision);
            tot -= val << i;
        }
    } while (tot);
    return 1;
}

void printmat2(cofft arr[3+NALG][2+NALG],int i0) {
    int i,j;
    for(i=i0;i<3+NALG;i++) {
        for(j=0;j<2+NALG;j++)
            printf(" %03d",arr[i][j]);
        printf("\n");
    }
}

void printmat(smultmat *mmat,int rnum) {
    printf("dat\n"); printmat2(mmat->dat,rnum);
    printf("msk\n"); printmat2(mmat->msk,rnum);
    printf("rem\n"); printmat2(mmat->rem,rnum);
    printf("sum\n"); printmat2(mmat->sum,rnum);
}

int SecmultSign(smultmat *mmat) {
    return mmat->sign;
}

void SecmultHandleRow(smultmat *mmat, int rownum, int allowCollision) {
    int i;

#if 0
    if(0==rownum) {
        printf("\n\nrownum=%d,allowCollision=%d\n",rownum,allowCollision);
        printmat(mmat,rownum);
    }
#endif

    if (rownum) {
        if (SecmultFirstRow(mmat,rownum,allowCollision))
            do {
                if( mmat->cols[rownum] ) {
                    unsigned int
                        idx = mmat->collisionidx,
                        collision = mmat->collision,
                        aux, bitidx;
                    if ( 0 != (collision & 1) ) {
                        /* Bockstein collisions not allowed */
                        continue;
                    }
                    /* try out the three possible consequences "v0", "vk", "wk" */
                    aux = (mmat->msk[rownum-1][idx+1] ^= collision);
                    if( 0 == (aux & (collision << 1)) ) {
                        mmat->msk[rownum-1][idx+1] |= (collision << 1);
                        mmat->decoration = 1; /* v0 */
                        SecmultHandleRow(mmat,rownum-1,0);
                        mmat->msk[rownum-1][idx+1] ^= (collision << 1);
                    }
                    for(bitidx=0,aux=collision;aux;aux>>=1) bitidx++;
                    if(bitidx>=2 && (0 == (mmat->msk[rownum-1][idx+bitidx] & 2))) {
                        mmat->msk[rownum-1][idx+bitidx] |= 2;
                        mmat->decoration = bitidx; /* vk */
                        SecmultHandleRow(mmat,rownum-1,0);
                        mmat->msk[rownum-1][idx+bitidx] ^= 2;
                    }
                    if(bitidx>=2 && (0 == (mmat->msk[rownum-1][idx+bitidx+1] & 1))) {
                        mmat->msk[rownum-1][idx+bitidx+1] |= 1;
                        mmat->decoration = 1-bitidx; /* wk */
                        SecmultHandleRow(mmat,rownum-1,0);
                        mmat->msk[rownum-1][idx+bitidx+1] ^= 1;
                    }
                    mmat->decoration = 0;
                } else {
                    SecmultHandleRow(mmat,rownum-1,allowCollision);
                }
            } while (SecmultNextRow(mmat,rownum,allowCollision));
    } else {
        /* collect results, if available */
        exmo res;
        res.ext = 0;
        res.coeff = (mmat->f1->coeff * mmat->f2->coeff) & 3;
        for(i=0;i<NALG;i++) {
            res.r.dat[i] = mmat->msk[0][i+2];
        }
	if (mmat->decoration) {
            unsigned int aux;
            if( mmat->decoration < 0) {
                aux = (1 << 4) | (-mmat->decoration);
            } else {
                if( mmat->decoration > 1) {
                    aux = (2 << 4) | (mmat->decoration-1);
                } else {
                    aux = 0;
                }
            }
            res.gen = (mmat->f2->gen & 0xffffff00) | aux ;
            res.coeff <<= 1;
	} else {
            if (SecmultSign(mmat)) res.coeff = -res.coeff & 3;
            res.gen = mmat->f2->gen;
	}
        PLappendExmo(mmat->ptp,mmat->pol,&res);
    }
}

void SecmultStart(Tcl_Interp *ip,
                  polyType *ptp, void *pol,
                  const exmo *f1, exmo *f2, int allowCollision) {
    int i;
    smultmat mmat;
    mmat.ip=ip;
    mmat.ptp=ptp;
    mmat.pol=pol;
    mmat.f1=f1;
    mmat.f2=f2;
    mmat.collision = 0;
    mmat.decoration = 0;
    for (i=0;i<NALG;i++) {
        mmat.rem[1+NALG][i+1] = f2->r.dat[i];
        mmat.sum[1+NALG][i] = 0;
        mmat.msk[1+NALG][i] = 0;
    }
    for (i=0;i<3+NALG;i++) {
        mmat.cols[i] = 0;
        mmat.msk[i][0] = 0;
    }
    SecmultHandleRow(&mmat,1+NALG,allowCollision);
}

void SecmultVCommute(Tcl_Interp *ip,
                     polyType *ptp, void *pol,
                     const exmo *f1, exmo *f2) {
    unsigned int idx = VWIDX(f1->gen),i,j,k;
    unsigned int fgen = f2->gen & 0xffffff00;

    /* vn */
    f2->gen = fgen | 0x20 | idx;
    SecmultStart(ip,ptp,pol,f1,f2,0);

    for(i=idx,j=0,k=1<<idx;i--;j++,k>>=1) {
        unsigned int aux =  f2->r.dat[j];
        if( aux >= k ) {
            f2->r.dat[j] = aux-k;
            f2->gen = fgen | 0x20 | i;
            SecmultStart(ip,ptp,pol,f1,f2,0);
            f2->r.dat[j] = aux;
        }
    }
}

void SecmultWCommute(Tcl_Interp *ip,
                     polyType *ptp, void *pol,
                     const exmo *f1, exmo *f2) {
    unsigned int idx = VWIDX(f1->gen),i,j,k;
    unsigned int fgen = f2->gen & 0xffffff00;

    /* wn */
    f2->gen = fgen | 0x10 | idx;
    SecmultStart(ip,ptp,pol,f1,f2,0);

    for(i=idx,j=0,k=1<<idx;--i;j++,k>>=1) {
        unsigned int aux =  f2->r.dat[j];
        if( aux >= k ) {
            f2->r.dat[j] = aux-k;
            f2->gen = fgen | 0x10 | i;
            SecmultStart(ip,ptp,pol,f1,f2,0);
            f2->r.dat[j] = aux;
        }
    }

    if( 0 != (f2->r.dat[idx-1] & 1) ) {
        /* v0 */
        f2->r.dat[idx-1] ^= 1;
        f2->gen = fgen | 0x20;
        SecmultStart(ip,ptp,pol,f1,f2,0);
        f2->r.dat[idx-1] ^= 1;        
    }

    if( 0 != (f2->r.dat[0] & 1) ) {

        f2->r.dat[0] ^= 1;

        /* vn */
        f2->gen = fgen | 0x20 | idx;
        SecmultStart(ip,ptp,pol,f1,f2,0);

        for(i=idx,j=0,k=1<<idx;i--;j++,k>>=1) {
            unsigned int aux =  f2->r.dat[j];
            if( aux >= k ) {
                f2->r.dat[j] = aux-k;
                f2->gen = fgen | 0x20 | i;
                SecmultStart(ip,ptp,pol,f1,f2,0);
                f2->r.dat[j] = aux;
            }
        }
  
    }

}

int SecmultExmo(Tcl_Interp *ip,
                polyType *ptp, void *pol,
                const exmo *f1, exmo *f2) {

    if (HASVW(f1->gen)) {
        if (HASVW(f2->gen)) {
            return TCL_OK;
        } else {
            if (0 == (1 & f2->coeff)) {
                /* second factor is even */
                return TCL_OK;
            }
            /* commute f1's vw through f2, then multiply mod 2 */
            if (HASV(f1->gen)) {
                SecmultVCommute(ip,ptp,pol,f1,f2);
            } else {
                SecmultWCommute(ip,ptp,pol,f1,f2);
            }
        }
    } else {
        if (HASVW(f2->gen)) {
            if (0 == (1 & f1->coeff)) {
                /* first factor is even */
                return TCL_OK;
            }
            /* multiply mod 2 */
            SecmultStart(ip,ptp,pol,f1,f2,0);
        } else {
            SecmultStart(ip,ptp,pol,f1,f2,1);
        }
    }

    return TCL_OK;
}

int Secmult(Tcl_Interp *ip,
            polyType *ptp1, void *pol1,
            polyType *ptp2, void *pol2,
            polyType **ptp3, void **pol3) {

    exmo f1, f2;
    int nsum1, nsum2, i, j;
    polyType *pt; void *pl;

    *ptp3 = pt = stdpoly;
    *pol3 = pl = PLcreate(*ptp3);

    nsum1 = PLgetNumsum(ptp1,pol1);
    nsum2 = PLgetNumsum(ptp2,pol2);

    for (i=0;i<nsum1;i++) {
        PLgetExmo(ptp1,pol1,&f1,i);
        for (j=0;j<nsum2;j++) {
            PLgetExmo(ptp2,pol2,&f2,j);
            if (TCL_OK != SecmultExmo(ip,pt,pl,&f1,&f2)) {
                return TCL_ERROR;
            }
        }
    }

    return TCL_OK;
}

int SecmultCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[]) {

    polyType *ptp1, *ptp2, *ptp3;
    void *pol1, *pol2, *pol3;

    if (objc != 3) {
        Tcl_WrongNumArgs(ip, 1, objv, "factor1 factor2");
        return TCL_ERROR;
    }

    if (TCL_OK != Tcl_ConvertToPoly(ip,objv[1])) {
        return TCL_ERROR;
    }

    ptp1 = polyTypeFromTclObj(objv[1]);
    pol1 = polyFromTclObj(objv[1]);

    if (TCL_OK != Tcl_ConvertToPoly(ip,objv[2])) {
        return TCL_ERROR;
    }

    ptp2 = polyTypeFromTclObj(objv[2]);
    pol2 = polyFromTclObj(objv[2]);

    if (TCL_OK != Secmult(ip,ptp1,pol1,ptp2,pol2,&ptp3,&pol3)) {
        PLfree(ptp3,pol3);
        return TCL_ERROR;
    }

    Tcl_SetObjResult(ip, Tcl_NewPolyObj(ptp3,pol3));
    return TCL_OK;
}

int Secmult2_Init(Tcl_Interp *ip) {

    Tcl_CreateObjCommand(ip, "steenrod::secmult2", SecmultCmd, (ClientData) 0, NULL);

    return TCL_OK;
}

