/*
 * Main entry point to the Steenrod library
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

#include <tcl.h>
#include <string.h>
#include "tprime.h"
#include "tpoly.h"
#include "tlin.h"
#include "steenrod.h"
#include "mult.h"


/* Our multiplication callback function. This interprets ma's client
 * data fields as follows:
 *
 *   ma->cd1 = matrixType
 *   ma->cd2 = matrix 
 *   ma->cd3 = destination enumerator
 *   ma->cd4 = error code  
 *   ma->cd5 = number of row that we're currently working on
 */

void addToMatrixCB(struct multArgs *ma, const exmo *smd) {
    matrixType *mtp = (matrixType *) ma->cd1;
    void *mat = ma->cd2;
    enumerator *dst = (enumerator *) ma->cd3;
    int row = (int) ma->cd5;
    int idx;
    int rcode;
    Tcl_Interp *ip = (Tcl_Interp *) ma->TclInterp;

    idx = SeqnoFromEnum(dst, (exmo *) smd);

    if (idx < 0) { 
        ma->cd4 = (void *) FAIL;
        goto error;
    }

    /* TODO: support optional checking whether dst[idx] really equals *smd */

    rcode = mtp->addToEntry(mat, row, idx, smd->coeff, ma->prime);

    if (SUCCESS == rcode)
        return;
   
    ma->cd4 = (void *) rcode;
   
 error:
 
    if (NULL != ip) {
        char err[200];
        Tcl_Obj *aux = Tcl_NewExmoCopyObj((exmo *) smd);
        sprintf(err, 
                "cannot account for monomial {%s}\n"
                "    (found sequence number %d)", 
                Tcl_GetString(aux), idx);
        Tcl_SetResult(ip, err, TCL_VOLATILE);
        Tcl_DecrRefCount(aux);
    }
}


#define RETERR(msg)\
{ if (NULL != ip) Tcl_SetResult(ip,msg,TCL_VOLATILE); return FAIL; }

int MakeMatrixSameSig(Tcl_Interp *ip, enumerator *src, momap *map, enumerator *dst, 
                      progressInfo *pinf, matrixType **mtp, void **mat) {
    
    int srcdim, dstdim;
    Tcl_Obj **dg = NULL;
    exmo theG; 
    polyType *dgpolyType;
    void *dgpoly;
    int dgispos, dgnumsum = 0; 
    multArgs ourMA, *ma = &ourMA;

    *mtp = NULL; *mat = NULL; /* if non-zero, caller will free this */

    /* check whether src and target are compatible */
    if ((NULL == src->pi) || (src->pi != dst->pi))
        RETERR("prime mismatch");

    /* see what kind of dg's we have to expect */
    if (src->ispos) {
        dgispos = dst->ispos;
    } else {
        if (dst->ispos) {
            RETERR("map from negative to positive not possible");
        } 
        dgispos = 1;
    }

    srcdim = DimensionFromEnum(src);
    dstdim = DimensionFromEnum(dst);

    *mtp = stdmatrix;
    *mat = stdmatrix->createMatrix(srcdim, dstdim);

    if (NULL == *mat) RETERR("out of memory");

    stdmatrix->clearMatrix(*mat);

    memset(&theG, 0, sizeof(exmo));
    theG.gen = -654321; /* invalid (=highly unusual) generator id */

    initMultargs(ma, src->pi, &(src->profile));

    ma->ffIsPos = src->ispos;
    ma->sfIsPos = dgispos;

    ma->getExmoFF = &stdGetSingleExmoFunc;
    ma->getExmoSF = &stdGetExmoFunc;

    ma->fetchFuncFF = &stdFetchFuncFF;
    ma->fetchFuncSF = &stdFetchFuncSF;

    ma->cd1 = *mtp;
    ma->cd2 = *mat;
    ma->cd3 = dst;
    ma->cd4 = SUCCESS;
    ma->cd5 = (void *) -1;
    ma->TclInterp = ip;
    ma->stdSummandFunc = &addToMatrixCB;

    if (firstRedmon(src)) 
        do {
            ++((int) ma->cd5); /* row indicator */

            /* find differential of src->theex'es generator */
            ma->ffdat = &(src->theex);
            ma->ffMaxLength = exmoGetRedLen(&(src->theex));
            ma->ffMaxLength = MIN(ma->ffMaxLength, NALG-2);

            if (theG.gen != src->theex.gen) {
                /* new generator: need to get its differential dg */

                theG.gen = src->theex.gen; 
                dg = momapGetValPtr(map, &theG);
                
                if (NULL == dg) continue;

                if (TCL_OK != Tcl_ConvertToPoly(ip, *dg)) {
                    char err[200];
                    sprintf(err,"target of generator #%d not of polynomial type", 
                            theG.gen);
                    RETERR(err);
                }

                dgpolyType = polyTypeFromTclObj(*dg);
                dgpoly     = polyFromTclObj(*dg);
                dgnumsum   = PLgetNumsum(dgpolyType, dgpoly);

                if (dgispos) {
                    if (SUCCESS != PLtest(dgpolyType, dgpoly, ISPOSITIVE)) { 
                        char err[200];
                        /* TODO: should we make sure and check each summand ? */ 
                        sprintf(err,"target of generator #%d not positive (?)", 
                                theG.gen);
                        RETERR(err);
                    }
                } else {
                    if (SUCCESS != PLtest(dgpolyType, dgpoly, ISNEGATIVE)) { 
                        char err[200];
                        /* TODO: should we make sure and check each summand ? */ 
                        sprintf(err,"target of generator #%d not negative (?)", 
                                theG.gen);
                        RETERR(err);
                    }
                }

                ma->sfdat  = dgpolyType; 
                ma->sfdat2 = dgpoly;
                ma->sfMaxLength = PLgetMaxRedLength(dgpolyType, dgpoly);
                ma->sfMaxLength = MIN(ma->sfMaxLength, NALG-2);

            }

            if (NULL == dg) continue;
     
            /* compute src->theex * dg */
            
            if (src->ispos)
                workPAchain(ma);
            else
                workAPchain(ma);

            multCount += dgnumsum;

            if (SUCCESS != (int) ma->cd4) {
                if (NULL != ip) {
                    char err[500];
                    Tcl_Obj *aux = Tcl_NewExmoCopyObj(&(src->theex));
                    sprintf(err, "\nwhile computing image of {%s}", 
                            Tcl_GetString(aux));
                    Tcl_DecrRefCount(aux);
                    Tcl_AddObjErrorInfo(ip, err, strlen(err));
                }
                return FAIL;
            }

        } while (nextRedmon(src));

    return SUCCESS;
}

int TMakeMatrixSameSig(ClientData cd, Tcl_Interp *ip,
                      int objc, Tcl_Obj * CONST objv[]) {

    enumerator *src, *dst;
    momap *map; 
    progressInfo info, *infoptr;
    matrixType *mtp;
    void *mat;

    if ((objc<4) || (objc>6)) {
        Tcl_WrongNumArgs(ip, 1, objv, 
                         "<enumerator> <monomap> <enumerator> ?<varname>? ?<int>?");
        return TCL_ERROR;
    }
    
    info.ip = ip; 
    info.progvar = NULL;
    info.pmsk = 0;
    infoptr = NULL;

    if (objc > 4) { 
        info.progvar = Tcl_GetString(objv[4]);
        infoptr = &info;
    }

    if (objc > 5) 
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[5], &info.pmsk))
            return TCL_ERROR;

    if (NULL == (src = Tcl_EnumFromObj(ip, objv[1]))) {
        Tcl_AppendResult(ip, " (argument #1)", NULL);
        return TCL_ERROR;
    }

    if (NULL == (map = Tcl_MomapFromObj(ip, objv[2]))) {
        Tcl_AppendResult(ip, " (argument #2)", NULL);
        return TCL_ERROR;
    }

    if (NULL == (dst = Tcl_EnumFromObj(ip, objv[3]))) {
        Tcl_AppendResult(ip, " (argument #3)", NULL);
        return TCL_ERROR;
    }

    if (SUCCESS != MakeMatrixSameSig(ip, src, map, dst, infoptr, &mtp, &mat)) {
        if (NULL != mat) mtp->destroyMatrix(mat);
        return TCL_ERROR;
    }
  
    Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mtp, mat));

    return TCL_OK;
}



/* Removing the string representation can sometimes help to save memory: */

#define STEALCOMMAND "StealStringRep"
int StealStringRep(ClientData cd, Tcl_Interp *ip,
                   int objc, Tcl_Obj * CONST objv[]) {
    Tcl_Obj *obj;

    if (objc != 2) 
        RETERR("usage: " STEALCOMMAND " <argument>");
    
    obj = objv[1];

    if (NULL != obj->typePtr) 
        if (NULL != obj->typePtr->updateStringProc)
            Tcl_InvalidateStringRep(obj);

    return TCL_OK;
}


int Steenrod_Init(Tcl_Interp *ip) {

    Tcl_InitStubs(ip, "8.0", 0);

    Tptr_Init(ip);
    Tprime_Init(ip);
    Tpoly_Init(ip);
    Momap_Init(ip);
    Tenum_Init(ip);
    Tlin_Init(ip);

    Tcl_CreateObjCommand(ip, POLYNSP "ComputeMatrix",
                         TMakeMatrixSameSig, (ClientData) 0, NULL);

    Tcl_CreateObjCommand(ip, STEALCOMMAND,
                         StealStringRep, (ClientData) 0, NULL);

    Tcl_Eval(ip, "namespace eval " POLYNSP " { namespace export * }");

#if 0
    /* We need to use a Tcl wrapper for platform specific library
     * loading, so "package provide" should be done in that wrapper. */
    Tcl_PkgProvide(ip, "Steenrod", "1.0");
#endif

    return TCL_OK;
}
