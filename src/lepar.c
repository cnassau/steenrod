/*
 * Lemon based general purpose parser
 *
 * Copyright (C) 2006 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <stdlib.h>

#include <tcl.h>

#include "lepar.h"

#define PARSERNAME  LePar
#define PARSERALLOC LeParAlloc
#define PARSERFREE  LeParFree
#define PARSERTRACE LeParTrace

void *PARSERALLOC(void *(*mallocProc)(size_t));
void  PARSERFREE(void *p, void (*freeproc)(void *));
void  PARSERTRACE(FILE *TraceFILE, char *zTracePrompt);
void  PARSERNAME(void *yyp, int yymajor, Tcl_Obj *yyminor, Parser *p);


#include "legram.h"

typedef struct {
    const char *name;
    int tokenid;
} tokdesc;

#define DECTOK(name, tok) { name, LEPAR_TK_ ## tok }
#define LEPAR_TK_FINAL -666

tokdesc toktab[] = {
    DECTOK("value", VALUE),
    DECTOK("function", FUNCTION),
    DECTOK("+", PLUS),
    DECTOK("-", MINUS),
    DECTOK("*", TIMES),
    DECTOK("/", DIVIDE),
    DECTOK("(", LPAREN),
    DECTOK(")", RPAREN),
    DECTOK("^", SUP),
    DECTOK("_", SUB),
    DECTOK(",", COMMA),
    DECTOK("eof", FINAL)
};

Parser *CreateParser(Tcl_Interp *ip) {
    int ntoks, i;
    Parser *res = malloc(sizeof(Parser));
    if (NULL == res) return NULL;

    res->deleted = res->busy = res->failed = res->tracing = 0;
    ntoks = res->numtokens = sizeof(toktab) / sizeof(tokdesc);
    res->tokens = (const char **) malloc((ntoks+1) * sizeof(const char *));

    for (i=0;i<ntoks;i++) {
        res->tokens[i] = toktab[i].name;
    }
    res->tokens[ntoks] = NULL;

    res->theparser = PARSERALLOC(malloc);
    if (NULL == res->theparser) {
        free(res);
        return NULL;
    }
    res->ip = ip;
    return res;
}

void DeleteParser(Parser * p) {
    if (p->busy) {
        /* don't destroy parser during use */
        p->failed = p->deleted = 1;
        return;
    }
    PARSERFREE(p->theparser, free);
    free(p->tokens);
    free(p);
}

typedef enum { PPARSE, PRESET, PFEED, PTRACE } pcmdcode;

static CONST char *pCmdNames[] = { "parse", "reset", "feed", "trace",
                                   (char *) NULL };

static pcmdcode pCmdmap[] = { PPARSE, PRESET, PFEED, PTRACE };

int ParserObjCmdProc(ClientData clientData,
                     Tcl_Interp *interp,
                     int objc,
                     Tcl_Obj *CONST objv[]) {

    Parser *p = (Parser *) clientData;
    Tcl_Interp *ip = p->ip;
    int result, index, rc, i;

    if (p->busy) {
        Tcl_SetResult(ip, "parser called recursively", TCL_STATIC);
        return TCL_ERROR;
    }

    p->busy = 1;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        goto error;
    }
   
    result = Tcl_GetIndexFromObj(ip, objv[1], pCmdNames, "subcommand", 0, &index);
    if (result != TCL_OK) 
        goto error;

    switch (pCmdmap[index]) {
        case PPARSE:

            break;

        case PTRACE:

            if (objc != 3) {
                Tcl_WrongNumArgs(ip, 2, objv, "on/off");
                goto error;
            }

            if (TCL_OK != Tcl_GetBooleanFromObj(ip, objv[2], &rc))
                goto error;

            if (rc) {
                p->tracing = 1;
                PARSERTRACE(stdout, "lemon: ");
            } else {
                p->tracing = 0;
                PARSERTRACE(NULL, NULL);
            }

            goto ok;

        case PRESET:
            p->failed = 0;

            goto ok;

        case PFEED:
      
            if (objc < 3) {
                Tcl_WrongNumArgs(ip, 2, objv, "token1 ?token2? ?token3? ...");
                goto error;
            }

            for (i=2; i<objc; i++) {
                Tcl_Obj *toktype;
                if (TCL_OK != Tcl_ListObjIndex(ip, objv[i], 0, &toktype))
                    goto error;
                result = Tcl_GetIndexFromObj(ip, toktype, p->tokens, "token", 0, &index);
                if (result != TCL_OK) 
                    goto error;
#if 0
                printf("%20s => Token %s (%d)\n",Tcl_GetString(objv[i]), 
                       toktab[index].name, toktab[index].tokenid);
#endif
                Tcl_IncrRefCount(objv[i]);
                if (LEPAR_TK_FINAL == toktab[index].tokenid) {
                    PARSERNAME(p->theparser, 0, NULL, p);
                } else {
                    PARSERNAME(p->theparser, toktab[index].tokenid, objv[i], p);
                }
                if (p->failed || p->deleted) {
                    goto error;
                }
            }

            goto ok;
    }

 error:
    rc = TCL_ERROR;
    if (p->deleted) {
        Tcl_SetResult(ip, "the parser has been deleted", TCL_STATIC);
    }
    goto cleanup;

 ok:
    rc = TCL_OK;
   
 cleanup:

    p->busy = 0;
    if (p->deleted) DeleteParser(p);

    return rc;
}

void ParserDeleteProc(ClientData clientData)
{
    DeleteParser((Parser *) clientData);
}  

int ParserCreateProc(ClientData clientData,
                     Tcl_Interp *interp,
                     int objc,
                     Tcl_Obj *CONST objv[]) {

    if (objc != 2) {
        char errmsg[200];
        sprintf(errmsg, "usage: %s <name>", Tcl_GetString(objv[0]));
        return TCL_ERROR;
    }

    Parser *pxx = CreateParser(interp);

    if (NULL == pxx) {
        return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp, Tcl_GetString(objv[1]), 
                         ParserObjCmdProc, pxx, ParserDeleteProc);
    return TCL_OK;
}


int Lepar_Init(Tcl_Interp *ip) {

    Tcl_InitStubs(ip, "8.0", 0);

    Tcl_CreateObjCommand(ip, "lepar::parser", ParserCreateProc, NULL, NULL);

    return TCL_OK;
}
