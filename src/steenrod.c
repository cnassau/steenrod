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
#include "tprime.h"
#include "tpoly.h"
#include "tlin.h"
#include "steenrod.h"

#define RETERR(msg)\
{ if (NULL != ip) Tcl_SetResult(ip,msg,TCL_VOLATILE); return FAIL; }

int MakeMatrixSameSig(Tcl_Interp *ip, enumerator *src, momap *map, enumerator *dst, 
                      progressInfo *pinf, matrixType **mtp, void **mat) {
    
    int srcdim, dstdim;

    /* check whether src and target are compatible */
    if ((NULL == src->pi) || (src->pi != dst->pi))
        RETERR("prime mismatch");

    srcdim = DimensionFromEnum(src);
    dstdim = DimensionFromEnum(dst);

    *mtp = stdmatrix;
    *mat = stdmatrix->createMatrix(srcdim, dstdim);

    if (NULL == *mat) RETERR("out of memory");

    stdmatrix->clearMatrix(*mat);



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

    if (SUCCESS != MakeMatrixSameSig(ip, src, map, dst, infoptr, &mtp, &mat))
        return TCL_ERROR;
   
    Tcl_SetObjResult(ip, Tcl_NewMatrixObj(mtp, mat));

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

    Tcl_Eval(ip, "namespace eval " POLYNSP " { namespace export * }");

    Tcl_PkgProvide(ip, "Steenrod", "1.0");

    return TCL_OK;
}
