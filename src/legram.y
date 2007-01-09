/*
 * Grammar file for the general purpose parser
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

%name LePar
%token_prefix    LEPAR_TK_

%left     AND.
%left     OR XOR.
%nonassoc EQ NE GT GE LT LE.
%left     PLUS MINUS.
%left     TIMES DIVIDE MOD.
%right    SUP SUB NOT.

%start_symbol result

%extra_argument {Parser *parser}

%token_type {Tcl_Obj *}

%token_destructor {
   if ($$) { Tcl_DecrRefCount($$); } ;
}

%parse_failure {
   Tcl_SetResult(parser->ip, "Syntax error", TCL_STATIC);
   parser->failed = 1;
}

%parse_accept {
   if( parser->failed ) Tcl_SetResult(parser->ip, "Exception encountered", TCL_STATIC);
}

%include {

#include <string.h> /* for memset */
#include "lepar.h"

Tcl_Obj *EvalN(Parser *p, const char *procname, int n) {
   Tcl_Obj *res;
   int rc,i;
   if( p->tracing) {
      printf("Calling \"%s\", %d argument%s\n",procname,n,(n>1) ? "s" : "");
      for(rc=0;rc<n;rc++)printf("  obj%d = %s\n",rc+1,Tcl_GetString(p->objv[rc+1]));
   }
   p->objv[0] = Tcl_NewStringObj(procname,-1);
   Tcl_IncrRefCount(p->objv[0]);
   if( !p->failed ) {
       rc = Tcl_EvalObjv(p->ip, 1+n, p->objv,0);
   } else {
       rc = TCL_ERROR;
   }
   if (TCL_OK != rc) {
       res = NULL;
   } else {
      res = Tcl_GetObjResult(p->ip);
      Tcl_IncrRefCount(res);
   }  
   for (i=0;i<=n;i++) Tcl_DecrRefCount(p->objv[i]);
   if (p->tracing) {
      printf("  result = %s\n", (NULL == res) ? "NULL" : Tcl_GetString(res));
   }
   return res;
}

#define CheckForError(A) {if (NULL == A) {parser->failed = 1;YYEXCEPTION;}}
   
 Tcl_Obj *ApplyFunc(Parser *parser, Tcl_Obj *funcname, 
                    Tcl_Obj *exp, Tcl_Obj *ind, Tcl_Obj *arglist) {
     Tcl_Obj *res;
     parser->objv[1]=funcname;

#define TRYSET(var,val) {                         \
 if (val) {var=val;}                              \
 else {var=Tcl_NewObj();Tcl_IncrRefCount(var);};}

     TRYSET(parser->objv[2],exp);
     TRYSET(parser->objv[3],ind);
     TRYSET(parser->objv[4],arglist);

     res = EvalN(parser,"apply",4);
     return res;
 }

 Tcl_Obj *Eval1(Parser *parser, const char *procname,
                Tcl_Obj *arg1) {
     parser->objv[1] = arg1; 
     return EvalN(parser,procname,1); 
 }

 Tcl_Obj *Eval2(Parser *parser, const char *procname,
                Tcl_Obj *arg1, Tcl_Obj *arg2) {
     parser->objv[1] = arg1; 
     parser->objv[2] = arg2; 
     return EvalN(parser,procname,2); 
 }

#define EV2(res,proc,in1,in2) \
{res=Eval2(parser,#proc,in1,in2);CheckForError(res);}
#define EV1(res,proc,in1) \
{res=Eval1(parser,#proc,in1);CheckForError(res);}

}

result(A) ::= expr(B) EOF. { 
   A = B;
   Tcl_SetObjResult(parser->ip,A);
   Tcl_DecrRefCount(B);
 }

expr(A) ::= expr(B) PLUS expr(C).  { EV2(A,plus,B,C); }
expr(A) ::= expr(B) MINUS expr(C).  { EV2(A,minus,B,C); }
expr(A) ::= expr(B) SUP expr(C).  { EV2(A,power,B,C); }
expr(A) ::= MINUS expr(B).  [NOT] { EV1(A,negate,B); }
expr(A) ::= expr(B) TIMES expr(C).  { EV2(A,times,B,C); }
expr(A) ::= expr(B) DIVIDE expr(C).  { EV2(A,divide,B,C); }
expr(A) ::= expr(B) MOD expr(C).  { EV2(A,modulo,B,C); }
expr(A) ::= expr(B) AND expr(C).  { EV2(A,and,B,C); }
expr(A) ::= expr(B) OR expr(C).  { EV2(A,or,B,C); }
expr(A) ::= expr(B) XOR expr(C).  { EV2(A,xor,B,C); }
expr(A) ::= LPAREN expr(B) RPAREN. { 
   A = B; B = NULL; 
   CheckForError(A);
}
expr(A) ::= expr(B) funcres(C).  { EV2(A,times,B,C); }
expr(A) ::= funcres(B). { A = B; B = NULL; }
expr(A) ::= VALUE(B). { EV1(A,value,B);  }
exprlist(A) ::= exprlist(B) COMMA expr(C). {
   A = B; 
   Tcl_ListObjAppendElement(parser->ip,A,C); 
   Tcl_DecrRefCount(C);
   // CheckForError(A); <-- can't happen
}
exprlist(A) ::= expr(B). {
   A = Tcl_NewListObj(1,&B);
   Tcl_DecrRefCount(B);
   if (A) Tcl_IncrRefCount(A);
   CheckForError(A);
}
/* brexprlist = bracketed expression list or immediate value */
brexprlist(A) ::= LPAREN exprlist(B) RPAREN. {
    A = B; B=NULL; 
}
brexprlist(A) ::= VALUE(B). {
    A = B; B=NULL; 
}
/* funcarglist = bracketed expression list or empty arglist */
funcarglist(A) ::= LPAREN exprlist(B) RPAREN. {
    A = B; B=NULL; 
}
funcarglist(A) ::= LPAREN RPAREN. {
    A = Tcl_NewObj(); 
    if(A) Tcl_IncrRefCount(A);
    CheckForError(A);
}
funcname(A) ::= FUNCTION(B). { EV1(A,function,B); }

funcres(A) ::= funcname(B) funcarglist(C). {
   A = ApplyFunc(parser,B,NULL,NULL,C);
   CheckForError(A);
}
funcres(A) ::= funcname(B) SUP brexprlist(exponent) SUB brexprlist(index) funcarglist(C). {
   A = ApplyFunc(parser,B,exponent,index,C);
   CheckForError(A);
}
funcres(A) ::= funcname(B) SUB brexprlist(index) SUP brexprlist(exponent) funcarglist(C). {
   A = ApplyFunc(parser,B,exponent,index,C);
   CheckForError(A);
}
funcres(A) ::= funcname(B) SUB brexprlist(index) funcarglist(C). {
   A = ApplyFunc(parser,B,NULL,index,C);
   CheckForError(A);
}
funcres(A) ::= funcname(B) SUP brexprlist(exponent) funcarglist(C). {
   A = ApplyFunc(parser,B,exponent,NULL,C);
   CheckForError(A);
}

%nonassoc DOLLAR FUNCTION
