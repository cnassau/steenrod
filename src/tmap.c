/*
 * Tcl interface to the implementation of maps
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

#include "tptr.h"
#include "tmap.h"
#include "maps.h"

/* copied these macros from tpoly.c */
#define RETERR(errmsg) \
{ Tcl_SetResult(ip, errmsg, TCL_VOLATILE) ; return TCL_ERROR; }

#define RETINT(i) { Tcl_SetObjResult(ip, Tcl_NewIntObj(i)); return TCL_OK; }

#define GETINT(ob, var)                                \
 if (TCL_OK != Tcl_GetIntFromObj(ip, ob, &privateInt)) \
    return TCL_ERROR;                                  \
 var = privateInt;

/* ... and these from tprime.c */
#define RETURNINT(rval) Tcl_SetObjResult(ip,Tcl_NewIntObj(rval)); return TCL_OK
#define RETURNLIST(list,len) \
Tcl_SetObjResult(ip,Tcl_ListFromArray(len,list)); return TCL_OK

/* implementation of the MAP_INFO command: */
int mkGenList(Tcl_Interp *ip, map *mp) {
    Tcl_Obj **obptr;
    Tcl_Obj *res;
    int i;
    obptr = malloc(mp->num * sizeof(Tcl_Obj *));
    if (NULL == obptr) RETERR("Out of memory");
    for (i=0;i<mp->num;i++) 
        obptr[i] = Tcl_NewTPtr(TP_MAPGEN, &(mp->dat[i]));
    res = Tcl_NewListObj(mp->num, obptr);
    free(obptr);
    Tcl_SetObjResult(ip, res);
    return TCL_OK;
}

typedef enum {
    MP_CREATE, MP_DESTROY, MP_INFO, MP_GETNUM,
    GEN_CREATE, GEN_FIND, GEN_INFO,
    GEN_SETIDEGREE, GEN_SETEDEGREE, 
    GEN_GETIDEGREE, GEN_GETEDEGREE,
    GEN_GETID, 
    SUM_INFO
} MapCmdCode;

int tMapCombiCmd(ClientData cd, Tcl_Interp *ip,
                 int objc, Tcl_Obj * CONST objv[]) {
    MapCmdCode cdi = (MapCmdCode) cd;

    map *mp;
    mapgen *mpg;

    int privateInt, ivar1, ivar2, ivar3;

    switch (cdi) {
        case MP_CREATE:    
            ENSUREARGS0;
            if (NULL == (mp = mapCreate())) 
                RETERR("Out of memory");
            Tcl_SetObjResult(ip, Tcl_NewTPtr(TP_MAP, mp));
            return TCL_OK;
        case MP_DESTROY:
            ENSUREARGS1(TP_MAP);
            mp = (map *) TPtr_GetPtr(objv[1]);
            mapDestroy(mp);
            return TCL_OK;
        case MP_GETNUM:
            ENSUREARGS1(TP_MAP);
            mp = (map *) TPtr_GetPtr(objv[1]);
            RETINT(mp->num);
        case MP_INFO:
            ENSUREARGS1(TP_MAP);
            mp = (map *) TPtr_GetPtr(objv[1]);
            /* return list of generators as TP_MAPGEN */
            return mkGenList(ip, mp);
        case GEN_CREATE:
            ENSUREARGS2(TP_MAP, TP_INT);
            mp = (map *) TPtr_GetPtr(objv[1]);
            GETINT(objv[2],ivar1);
            if (SUCCESS != mapAddGen(mp, ivar1)) 
                RETERR("problem in mapAddGen");
            /* fall through to return a reference to the new generator */
        case GEN_FIND:
            ENSUREARGS2(TP_MAP, TP_INT);
            mp = (map *) TPtr_GetPtr(objv[1]);
            GETINT(objv[2],ivar1);
            if (NULL == (mpg = mapFindGen(mp, ivar1)))
                return TCL_OK;
            Tcl_SetObjResult(ip, Tcl_NewTPtr(TP_MAPGEN, mpg));
            return TCL_OK;
        case GEN_SETIDEGREE:
            ENSUREARGS2(TP_MAPGEN, TP_INT);
            mpg = (mapgen *) TPtr_GetPtr(objv[1]);
            GETINT(objv[2],ivar1);
            mpg->ideg = ivar1;
            return TCL_OK;            
        case GEN_SETEDEGREE:
            ENSUREARGS2(TP_MAPGEN, TP_INT);
            mpg = (mapgen *) TPtr_GetPtr(objv[1]);
            GETINT(objv[2],ivar1);
            mpg->edeg = ivar1;
            return TCL_OK;            
        case GEN_GETIDEGREE:
            ENSUREARGS1(TP_MAPGEN); 
            mpg = (mapgen *) TPtr_GetPtr(objv[1]);
            RETINT(mpg->ideg);
        case GEN_GETEDEGREE:
            ENSUREARGS1(TP_MAPGEN); 
            mpg = (mapgen *) TPtr_GetPtr(objv[1]);
            RETINT(mpg->edeg);
        case GEN_GETID:
            ENSUREARGS1(TP_MAPGEN); 
            mpg = (mapgen *) TPtr_GetPtr(objv[1]);
            RETINT(mpg->id);
        case GEN_INFO:
            return TCL_OK;
        case SUM_INFO:
            return TCL_OK;
        case 550:
            return TCL_OK;
    }

    RETERR("tMapCombiCmd: internal error"); 
}    


int Tmap_HaveType;

int Tmap_Init(Tcl_Interp *ip) {

    Tcl_InitStubs(ip, "8.0", 0);

    Tptr_Init(ip);

    if (!Tmap_HaveType) {
        TPtr_RegType(TP_MAP,    "map");
        TPtr_RegType(TP_MAPGEN, "map_generator");
        TPtr_RegType(TP_MAPSUM, "map_summand");
        Tmap_HaveType = 1;
    }

#define CREATECOMMAND(name, code) \
Tcl_CreateObjCommand(ip,name,tMapCombiCmd,(ClientData) code, NULL);

#define NSM "_map::"

    CREATECOMMAND(NSM "createMap",  MP_CREATE);
    CREATECOMMAND(NSM "destroyMap", MP_DESTROY);
    CREATECOMMAND(NSM "infoMap",    MP_INFO);
    CREATECOMMAND(NSM "getNumGens",     MP_GETNUM);
    CREATECOMMAND(NSM "createGen",      GEN_CREATE);
    CREATECOMMAND(NSM "genGetId",       GEN_GETID);
    CREATECOMMAND(NSM "genSetIDegree",  GEN_SETIDEGREE);
    CREATECOMMAND(NSM "genSetEDegree",  GEN_SETEDEGREE);
    CREATECOMMAND(NSM "genGetIDegree",  GEN_GETIDEGREE);
    CREATECOMMAND(NSM "genGetEDegree",  GEN_GETEDEGREE);
    CREATECOMMAND(NSM "findGen",    GEN_FIND);
    CREATECOMMAND(NSM "infoGen",    GEN_INFO);

    return TCL_OK;
}


