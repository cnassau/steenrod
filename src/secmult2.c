/*
 * Secondary multiplication routine, prime 2
 *
 * Copyright (C) 2004-2006 Christian Nassau <nassau@nullhomotopie.de>
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

int SecmultExmo(Tcl_Interp *ip,
            polyType *ptp1, void *pol1, 
            exmo *f1, exmo *f2) {

    

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
        for (j=0;j<nsum2;j++) {
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

