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
#include "tprofile.h"
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
    obptr = mallox(mp->num * sizeof(Tcl_Obj *));
    if (NULL == obptr) RETERR("Out of memory");
    for (i=0;i<mp->num;i++) 
        obptr[i] = Tcl_NewTPtr(TP_MAPGEN, &(mp->dat[i]));
    res = Tcl_NewListObj(mp->num, obptr);
    free(obptr);
    Tcl_SetObjResult(ip, res);
    return TCL_OK;
}

/* implementation of the getTarget command */
int getTarget(Tcl_Interp *ip, mapgen *mpg) {
    Tcl_Obj **obptr;
    Tcl_Obj *res;
    int i;
    obptr = mallox(mpg->num * sizeof(Tcl_Obj *));
    if (NULL == obptr) RETERR("Out of memory");
    for (i=0;i<mpg->num;i++) 
        obptr[i] = Tcl_NewTPtr(TP_MAPSUM, &(mpg->dat[i]));
    res = Tcl_NewListObj(mpg->num, obptr);
    free(obptr);
    Tcl_SetObjResult(ip, res);
    return TCL_OK;
}

/* implementation of getSumData */
int getSumData(Tcl_Interp *ip, mapsum *mps) {
    Tcl_Obj **obptr;
    Tcl_Obj *(res[6]), *rval;
    int i,j,k, sz;
    cint *cptr = mps->cdat; 
    xint *xptr = mps->xdat;
    sz = mps->num * (mps->len + 1);
    obptr = mallox(sz * sizeof(Tcl_Obj *));
    if (NULL == obptr) RETERR("Out of memory");
    res[0] = Tcl_NewIntObj(mps->gen);
    res[1] = Tcl_NewIntObj(mps->edat);
    res[2] = Tcl_NewIntObj(mps->pad);
    res[3] = Tcl_NewIntObj(mps->len);
    res[4] = Tcl_NewIntObj(0);
    for (i=0,j=0;i<mps->num;i++) { 
        obptr[j++] = Tcl_NewIntObj(*cptr++);
        for (k=0;k<mps->len;k++) 
            obptr[j++] = Tcl_NewIntObj(*xptr++);
    }
    res[5] = Tcl_NewListObj(sz, obptr);
    free(obptr);
    rval   = Tcl_NewListObj(6, res);
    Tcl_SetObjResult(ip, rval);
    return TCL_OK;
}


typedef enum {
    MP_CREATE, MP_DESTROY, MP_INFO, MP_GETNUM, 
    MP_GETMAXGEN, MP_GETMINIDEG, MP_GETMAXIDEG,
    MP_CREATESQN, MP_DESTROYSQN, 
    MP_FIRSTWITHSQN, MP_NEXTWITHSQN,
    GEN_CREATE, GEN_FIND, 
    GEN_SETIDEGREE, GEN_SETEDEGREE, 
    GEN_GETIDEGREE, GEN_GETEDEGREE,
    GEN_GETID, GEN_GETTARGET, GEN_APPENDTARGET,
    SUM_GETDATA
} MapCmdCode;

int tMapCombiCmd(ClientData cd, Tcl_Interp *ip,
                 int objc, Tcl_Obj * CONST objv[]) {
    MapCmdCode cdi = (MapCmdCode) cd;

    map *mp;
    mapgen *mpg;
    mapsum *mps;
    mapsqndata *mpsd;
    enumEnv *env;
    exmon *exm1, *exm2;

    int privateInt, ivar1, ivar2;

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
        case MP_GETMAXGEN:
            ENSUREARGS1(TP_MAP);
            mp = (map *) TPtr_GetPtr(objv[1]);
            RETINT(mp->maxgen);
        case MP_GETMINIDEG:
            ENSUREARGS1(TP_MAP);
            mp = (map *) TPtr_GetPtr(objv[1]);
            RETINT(mapGetMinIdeg(mp));
        case MP_GETMAXIDEG:
            ENSUREARGS1(TP_MAP);
            mp = (map *) TPtr_GetPtr(objv[1]);
            RETINT(mapGetMaxIdeg(mp));
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
        case GEN_GETTARGET: 
            ENSUREARGS1(TP_MAPGEN);
            mpg = (mapgen *) TPtr_GetPtr(objv[1]);
            return getTarget(ip, mpg);
        case SUM_GETDATA: 
            ENSUREARGS1(TP_MAPSUM);
            mps = (mapsum *) TPtr_GetPtr(objv[1]);
            return getSumData(ip, mps);
        case GEN_APPENDTARGET:
            ENSUREARGS7(TP_MAPGEN,TP_INT,TP_INT,TP_INT,TP_INT,TP_INT,TP_INTLIST);
            mpg = (mapgen *) TPtr_GetPtr(objv[1]);
            GETINT(objv[2],ivar1); /* target gen */
            GETINT(objv[3],ivar2); /* exterior data */
            mps = mapgenFindSum(mpg, ivar1, ivar2);
            if (NULL == mps) mps = mapgenCreateSum(mpg, ivar1, ivar2);
            if (NULL == mps) RETERR("could not create target component");
            GETINT(objv[4],ivar1); /* pad */
            if (SUCCESS != mapsumSetPad(mps, ivar1)) RETERR("padding conflict");
            GETINT(objv[5],ivar2); /* len */
            GETINT(objv[6],ivar1); /* data format */
            if (0 != ivar1) RETERR("unknown data format");
            {
                int obc; Tcl_Obj **obv; int len = ivar2;
                Tcl_ListObjGetElements(ip, objv[7], &obc, &obv);
                if (0 != (obc % (len + 1))) RETERR("number of ints inconsistent");
                if (SUCCESS != mapsumAppendFromList(mps, len, obc, obv))
                    RETERR("mapsumAppendFromList failed");
            }
            return TCL_OK;       
        case MP_CREATESQN:    
            ENSUREARGS4(TP_MAP,TP_ENENV,TP_INT,TP_INT);
            mp = (map *) TPtr_GetPtr(objv[1]);
            env = (enumEnv *) TPtr_GetPtr(objv[2]);
            GETINT(objv[3],ivar1);
            GETINT(objv[4],ivar2);
            if (NULL == (mpsd = mapCreateSqnData(mp,env,ivar1,ivar2))) 
                RETERR("Out of memory");
            Tcl_SetObjResult(ip, Tcl_NewTPtr(TP_MAPSQD, mpsd));
            return TCL_OK;
        case MP_DESTROYSQN:
            ENSUREARGS1(TP_MAPSQD);
            mpsd = (mapsqndata *) TPtr_GetPtr(objv[1]);
            mapDestroySqnData(mpsd);
            return TCL_OK;
        case MP_FIRSTWITHSQN:
            ENSUREARGS3(TP_MAPSQD, TP_EXMON, TP_EXMON);
            mpsd = (mapsqndata *) TPtr_GetPtr(objv[1]);
            exm1 = (exmon *) TPtr_GetPtr(objv[2]);
            exm2 = (exmon *) TPtr_GetPtr(objv[3]);
            Tcl_SetObjResult(ip, Tcl_NewBooleanObj(MSDfirst(mpsd, exm1, exm2)));
            return TCL_OK;
        case MP_NEXTWITHSQN:
            ENSUREARGS3(TP_MAPSQD, TP_EXMON, TP_EXMON);
            mpsd = (mapsqndata *) TPtr_GetPtr(objv[1]);
            exm1 = (exmon *) TPtr_GetPtr(objv[2]);
            exm2 = (exmon *) TPtr_GetPtr(objv[3]);
            Tcl_SetObjResult(ip, Tcl_NewBooleanObj(MSDnext(mpsd, exm1, exm2)));
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
        TPtr_RegType(TP_MAPSQD, "map_seqno_data");
        Tmap_HaveType = 1;
    }

#define CREATECOMMAND(name, code) \
Tcl_CreateObjCommand(ip,name,tMapCombiCmd,(ClientData) code, NULL);

#define NSM "_map::"

    CREATECOMMAND(NSM "createMap",      MP_CREATE);
    CREATECOMMAND(NSM "destroyMap",     MP_DESTROY);
    CREATECOMMAND(NSM "infoMap",        MP_INFO);
    CREATECOMMAND(NSM "getNumGens",     MP_GETNUM);
    CREATECOMMAND(NSM "getMaxGen",      MP_GETMAXGEN);
    CREATECOMMAND(NSM "getMaxIDeg",     MP_GETMAXIDEG);
    CREATECOMMAND(NSM "getMinIDeg",     MP_GETMINIDEG);
    CREATECOMMAND(NSM "createGen",      GEN_CREATE);
    CREATECOMMAND(NSM "genGetId",       GEN_GETID);
    CREATECOMMAND(NSM "genSetIDegree",  GEN_SETIDEGREE);
    CREATECOMMAND(NSM "genSetEDegree",  GEN_SETEDEGREE);
    CREATECOMMAND(NSM "genGetIDegree",  GEN_GETIDEGREE);
    CREATECOMMAND(NSM "genGetEDegree",  GEN_GETEDEGREE);
    CREATECOMMAND(NSM "findGen",        GEN_FIND);
    CREATECOMMAND(NSM "getTarget",      GEN_GETTARGET);
    CREATECOMMAND(NSM "appendTarget",   GEN_APPENDTARGET);
    CREATECOMMAND(NSM "getSumData",     SUM_GETDATA);
    CREATECOMMAND(NSM "createMapSqnData",  MP_CREATESQN);
    CREATECOMMAND(NSM "destroyMapSqnData", MP_DESTROYSQN);
    CREATECOMMAND(NSM "msdFirst",       MP_FIRSTWITHSQN);
    CREATECOMMAND(NSM "msdNext",        MP_NEXTWITHSQN);

    return TCL_OK;
}


