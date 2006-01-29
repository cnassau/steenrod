/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is include which follows the "include" declaration
** in the input file. */
#include <stdio.h>
#line 43 "legram.y"


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
   for (i=0;i<=n;i++) Tcl_DecrRefCount(p->objv[i]);
   if (TCL_OK != rc) {
      res = NULL;
   } else {
      res = Tcl_GetObjResult(p->ip);
      Tcl_IncrRefCount(res);
   }
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
     parser->objv[2]=arglist;
     res = EvalN(parser,"apply",2);
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

#line 71 "legram.c"
/* Next is all token values, in a form suitable for use by makeheaders.
** This section will be null unless lemon is run with the -m switch.
*/
/* 
** These constants (all generated automatically by the parser generator)
** specify the various kinds of tokens (terminals) that the parser
** understands. 
**
** Each symbol here is a terminal symbol in the grammar.
*/
/* Make sure the INTERFACE macro is defined.
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/* The next thing included is series of defines which control
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 terminals
**                       and nonterminals.  "int" is used otherwise.
**    YYNOCODE           is a number of type YYCODETYPE which corresponds
**                       to no legal terminal or nonterminal number.  This
**                       number is used to fill in empty slots of the hash 
**                       table.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       have fall-back values which should be used if the
**                       original value of the token will not parse.
**    YYACTIONTYPE       is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 rules and
**                       states combined.  "int" is used otherwise.
**    LeParTOKENTYPE     is the data type used for minor tokens given 
**                       directly to the parser from the tokenizer.
**    YYMINORTYPE        is the data type used for all minor tokens.
**                       This is typically a union of many types, one of
**                       which is LeParTOKENTYPE.  The entry in the union
**                       for base tokens is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.
**    LeParARG_SDECL     A static variable declaration for the %extra_argument
**    LeParARG_PDECL     A parameter declaration for the %extra_argument
**    LeParARG_STORE     Code to store %extra_argument into yypParser
**    LeParARG_FETCH     Code to extract %extra_argument from yypParser
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
*/
#define YYCODETYPE unsigned char
#define YYNOCODE 32
#define YYACTIONTYPE unsigned char
#define LeParTOKENTYPE Tcl_Obj *
typedef union {
  LeParTOKENTYPE yy0;
  int yy63;
} YYMINORTYPE;
#define YYSTACKDEPTH 100
#define LeParARG_SDECL Parser *parser;
#define LeParARG_PDECL ,Parser *parser
#define LeParARG_FETCH Parser *parser = yypParser->parser
#define LeParARG_STORE yypParser->parser = parser
#define YYNSTATE 52
#define YYNRULE 22
#define YYERRORSYMBOL 25
#define YYERRSYMDT yy63
#define YY_NO_ACTION      (YYNSTATE+YYNRULE+2)
#define YY_ACCEPT_ACTION  (YYNSTATE+YYNRULE+1)
#define YY_ERROR_ACTION   (YYNSTATE+YYNRULE)

/* Next are that tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N < YYNSTATE                  Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   YYNSTATE <= N < YYNSTATE+YYNRULE   Reduce by rule N-YYNSTATE.
**
**   N == YYNSTATE+YYNRULE              A syntax error has occurred.
**
**   N == YYNSTATE+YYNRULE+1            The parser accepts its input.
**
**   N == YYNSTATE+YYNRULE+2            No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as
**
**      yy_action[ yy_shift_ofst[S] + X ]
**
** If the index value yy_shift_ofst[S]+X is out of range or if the value
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X or if yy_shift_ofst[S]
** is equal to YY_SHIFT_USE_DFLT, it means that the action is not in the table
** and that yy_default[S] should be used instead.  
**
** The formula above is for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
*/
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    11,   12,   13,    5,    6,    8,    9,   10,    7,    5,
 /*    10 */     6,    8,    9,   10,    7,   14,   44,   42,   33,   11,
 /*    20 */    12,   13,   44,   15,   43,   47,   31,   44,    5,    6,
 /*    30 */     8,    9,   10,    7,   11,   12,   13,   39,   45,   24,
 /*    40 */    46,   44,   31,    5,    6,    8,    9,   10,    7,   12,
 /*    50 */    13,   19,   46,   32,   31,    2,   44,    5,    6,    8,
 /*    60 */     9,   10,    7,    8,    9,   10,    7,   41,   55,    4,
 /*    70 */    44,   75,   17,   46,   44,   31,   19,   46,   34,   31,
 /*    80 */    19,   46,   35,   31,   19,   46,   36,   31,   25,   46,
 /*    90 */    55,   31,   26,   46,   55,   31,   37,   40,   27,   46,
 /*   100 */     1,   31,   28,   46,    7,   31,   29,   46,   55,   31,
 /*   110 */    21,   46,   44,   31,   22,   46,   55,   31,   38,   23,
 /*   120 */    46,    3,   31,   30,   46,   55,   31,   18,   46,   55,
 /*   130 */    31,   20,   46,   48,   31,   16,   49,   55,   16,   50,
 /*   140 */    51,   16,   16,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */     1,    2,    3,   10,   11,   12,   13,   14,   15,   10,
 /*    10 */    11,   12,   13,   14,   15,   11,   23,   18,   21,    1,
 /*    20 */     2,    3,   23,   19,   28,   21,   30,   23,   10,   11,
 /*    30 */    12,   13,   14,   15,    1,    2,    3,   21,   20,   27,
 /*    40 */    28,   23,   30,   10,   11,   12,   13,   14,   15,    2,
 /*    50 */     3,   27,   28,   29,   30,   19,   23,   10,   11,   12,
 /*    60 */    13,   14,   15,   12,   13,   14,   15,   21,   31,   19,
 /*    70 */    23,   26,   27,   28,   23,   30,   27,   28,   29,   30,
 /*    80 */    27,   28,   29,   30,   27,   28,   29,   30,   27,   28,
 /*    90 */    31,   30,   27,   28,   31,   30,   15,   16,   27,   28,
 /*   100 */    19,   30,   27,   28,   15,   30,   27,   28,   31,   30,
 /*   110 */    27,   28,   23,   30,   27,   28,   31,   30,   16,   27,
 /*   120 */    28,   19,   30,   27,   28,   31,   30,   27,   28,   31,
 /*   130 */    30,   27,   28,   20,   30,   22,   20,   31,   22,   20,
 /*   140 */    20,   22,   22,
};
#define YY_SHIFT_USE_DFLT (-8)
#define YY_SHIFT_MAX 41
static const signed char yy_shift_ofst[] = {
 /*     0 */     4,    4,    4,    4,    4,    4,    4,    4,    4,    4,
 /*    10 */     4,    4,    4,    4,    4,    4,    4,   -1,   18,   33,
 /*    20 */    33,   47,   -7,   -7,   51,   51,   89,   89,   89,   89,
 /*    30 */    89,   81,  113,  102,  116,  119,  120,   -3,   16,   36,
 /*    40 */    46,   50,
};
#define YY_REDUCE_USE_DFLT (-5)
#define YY_REDUCE_MAX 30
static const signed char yy_reduce_ofst[] = {
 /*     0 */    45,   24,   49,   53,   57,   12,   61,   65,   71,   75,
 /*    10 */    79,   83,   87,   92,   96,  100,  104,   -4,   -4,   -4,
 /*    20 */    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,
 /*    30 */    -4,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */    74,   74,   74,   74,   74,   74,   74,   74,   74,   74,
 /*    10 */    74,   74,   74,   74,   74,   74,   74,   74,   74,   68,
 /*    20 */    67,   60,   61,   62,   53,   54,   55,   57,   58,   59,
 /*    30 */    56,   74,   74,   74,   74,   74,   74,   74,   74,   74,
 /*    40 */    74,   74,   52,   64,   69,   63,   65,   66,   70,   71,
 /*    50 */    73,   72,
};
#define YY_SZ_ACTTAB (sizeof(yy_action)/sizeof(yy_action[0]))

/* The next table maps tokens into fallback tokens.  If a construct
** like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammer, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
*/
struct yyStackEntry {
  int stateno;       /* The state-number */
  int major;         /* The major token value.  This is the code
                     ** number for the token at this stack level */
  YYMINORTYPE minor; /* The user-supplied minor token value.  This
                     ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  int yyidx;                    /* Index of top element in stack */
  int yyerrcnt;                 /* Shifts left before out of the error */
  LeParARG_SDECL                /* A place to hold %extra_argument */
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void LeParTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if(yyTraceFILE==0) yyTracePrompt = 0;
  else if(yyTracePrompt==0) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "AND",           "OR",            "XOR",         
  "EQ",            "NE",            "GT",            "GE",          
  "LT",            "LE",            "PLUS",          "MINUS",       
  "TIMES",         "DIVIDE",        "MOD",           "SUP",         
  "SUB",           "NOT",           "EOF",           "LPAREN",      
  "RPAREN",        "VALUE",         "COMMA",         "FUNCTION",    
  "DOLLAR",        "error",         "result",        "expr",        
  "funcres",       "exprlist",      "funcname",    
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "result ::= expr EOF",
 /*   1 */ "expr ::= expr PLUS expr",
 /*   2 */ "expr ::= expr MINUS expr",
 /*   3 */ "expr ::= expr SUP expr",
 /*   4 */ "expr ::= MINUS expr",
 /*   5 */ "expr ::= expr TIMES expr",
 /*   6 */ "expr ::= expr DIVIDE expr",
 /*   7 */ "expr ::= expr MOD expr",
 /*   8 */ "expr ::= expr AND expr",
 /*   9 */ "expr ::= expr OR expr",
 /*  10 */ "expr ::= expr XOR expr",
 /*  11 */ "expr ::= LPAREN expr RPAREN",
 /*  12 */ "expr ::= expr funcres",
 /*  13 */ "expr ::= funcres",
 /*  14 */ "expr ::= VALUE",
 /*  15 */ "exprlist ::= exprlist COMMA expr",
 /*  16 */ "exprlist ::= expr",
 /*  17 */ "funcname ::= FUNCTION",
 /*  18 */ "funcres ::= funcname LPAREN exprlist RPAREN",
 /*  19 */ "funcres ::= funcname SUP VALUE SUB VALUE LPAREN exprlist RPAREN",
 /*  20 */ "funcres ::= funcname SUB VALUE LPAREN exprlist RPAREN",
 /*  21 */ "funcres ::= funcname SUP VALUE LPAREN exprlist RPAREN",
};
#endif /* NDEBUG */

/*
** This function returns the symbolic name associated with a token
** value.
*/
const char *LeParTokenName(int tokenType){
#ifndef NDEBUG
  if(tokenType>0 && tokenType<(sizeof(yyTokenName)/sizeof(yyTokenName[0]))){
    return yyTokenName[tokenType];
  }else{
    return "Unknown";
  }
#else
  return "";
#endif
}

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to LePar and LeParFree.
*/
void *LeParAlloc(void *(*mallocProc)(size_t)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)((size_t)sizeof(yyParser));
  if(pParser){
    pParser->yyidx = -1;
  }
  return pParser;
}

/* The following function deletes the value associated with a
** symbol.  The symbol can be either a terminal or nonterminal.
** "yymajor" is the symbol code, and "yypminor" is a pointer to
** the value.
*/
static void yy_destructor(YYCODETYPE yymajor, YYMINORTYPE *yypminor){
  switch(yymajor){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are not used
    ** inside the C code.
    */
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
#line 30 "legram.y"
{
   if ((yypminor->yy0)) Tcl_DecrRefCount((yypminor->yy0));
}
#line 453 "legram.c"
      break;
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
**
** Return the major token number for the symbol popped.
*/
static int yy_pop_parser_stack(yyParser *pParser){
  YYCODETYPE yymajor;
  yyStackEntry *yytos = &pParser->yystack[pParser->yyidx];

  if(pParser->yyidx<0) return 0;
#ifndef NDEBUG
  if(yyTraceFILE && pParser->yyidx>=0){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yymajor = yytos->major;
  yy_destructor(yymajor, &yytos->minor);
  pParser->yyidx--;
  return yymajor;
}

/* 
** Deallocate and destroy a parser.  Destructors are all called for
** all stack elements before shutting the parser down.
**
** Inputs:
** <ul>
** <li>  A pointer to the parser.  This should be a pointer
**       obtained from LeParAlloc.
** <li>  A pointer to a function used to reclaim memory obtained
**       from malloc.
** </ul>
*/
void LeParFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
  if(pParser==0) return;
  while(pParser->yyidx>=0) yy_pop_parser_stack(pParser);
  (*freeProc)((void*)pParser);
}

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  int iLookAhead            /* The look-ahead token */
){
  int i;
  int stateno = pParser->yystack[pParser->yyidx].stateno;
 
  if(stateno>YY_SHIFT_MAX || (i = yy_shift_ofst[stateno])==YY_SHIFT_USE_DFLT){
    return yy_default[stateno];
  }
  if(iLookAhead==YYNOCODE){
    return YY_NO_ACTION;
  }
  i += iLookAhead;
  if(i<0 || i>=YY_SZ_ACTTAB || yy_lookahead[i]!=iLookAhead){
#ifdef YYFALLBACK
    int iFallback;            /* Fallback token */
    if(iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
           && (iFallback = yyFallback[iLookAhead])!=0){
#ifndef NDEBUG
      if(yyTraceFILE){
        fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
           yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
      }
#endif
      return yy_find_shift_action(pParser, iFallback);
    }
#endif
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  int iLookAhead            /* The look-ahead token */
){
  int i;
  /* int stateno = pParser->yystack[pParser->yyidx].stateno; */
 
  if(stateno>YY_REDUCE_MAX ||
      (i = yy_reduce_ofst[stateno])==YY_REDUCE_USE_DFLT){
    return yy_default[stateno];
  }
  if(iLookAhead==YYNOCODE){
    return YY_NO_ACTION;
  }
  i += iLookAhead;
  if(i<0 || i>=YY_SZ_ACTTAB || yy_lookahead[i]!=iLookAhead){
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  YYMINORTYPE *yypMinor         /* Pointer ot the minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yyidx++;
  if(yypParser->yyidx>=YYSTACKDEPTH){
     LeParARG_FETCH;
     yypParser->yyidx--;
#ifndef NDEBUG
     if(yyTraceFILE){
       fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
     }
#endif
     while(yypParser->yyidx>=0) yy_pop_parser_stack(yypParser);
     /* Here code is inserted which will execute if the parser
     ** stack every overflows */
     LeParARG_STORE; /* Suppress warning about unused %extra_argument var */
     return;
  }
  yytos = &yypParser->yystack[yypParser->yyidx];
  yytos->stateno = yyNewState;
  yytos->major = yyMajor;
  yytos->minor = *yypMinor;
#ifndef NDEBUG
  if(yyTraceFILE && yypParser->yyidx>0){
    int i;
    fprintf(yyTraceFILE,"%sShift %d\n",yyTracePrompt,yyNewState);
    fprintf(yyTraceFILE,"%sStack:",yyTracePrompt);
    for(i=1; i<=yypParser->yyidx; i++)
      fprintf(yyTraceFILE," %s",yyTokenName[yypParser->yystack[i].major]);
    fprintf(yyTraceFILE,"\n");
  }
#endif
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 26, 2 },
  { 27, 3 },
  { 27, 3 },
  { 27, 3 },
  { 27, 2 },
  { 27, 3 },
  { 27, 3 },
  { 27, 3 },
  { 27, 3 },
  { 27, 3 },
  { 27, 3 },
  { 27, 3 },
  { 27, 2 },
  { 27, 1 },
  { 27, 1 },
  { 29, 3 },
  { 29, 1 },
  { 30, 1 },
  { 28, 4 },
  { 28, 8 },
  { 28, 6 },
  { 28, 6 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  int yyruleno                 /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  YYMINORTYPE yygotominor;        /* The LHS of the rule reduced */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  LeParARG_FETCH;
  yymsp = &yypParser->yystack[yypParser->yyidx];
#ifndef NDEBUG
  if(yyTraceFILE && yyruleno>=0 
        && yyruleno<sizeof(yyRuleName)/sizeof(yyRuleName[0])){
    fprintf(yyTraceFILE, "%sReduce [%s].\n", yyTracePrompt,
      yyRuleName[yyruleno]);
  }
#endif /* NDEBUG */

#ifndef NDEBUG
  /* Silence complaints from purify about yygotominor being uninitialized
  ** in some cases when it is copied into the stack after the following
  ** switch.  yygotominor is uninitialized when a rule reduces that does
  ** not set the value of its left-hand side nonterminal.  Leaving the
  ** value of the nonterminal uninitialized is utterly harmless as long
  ** as the value is never used.  So really the only thing this code
  ** accomplishes is to quieten purify.  
  */
  memset(&yygotominor, 0, sizeof(yygotominor));
#endif

  /* cna: allow rules to raise exceptions (eg. divison by zero) */
  int yyexception = 0; 
#define YYEXCEPTION {yyexception = 1;}

  switch(yyruleno){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
      case 0:
#line 106 "legram.y"
{ 
   yygotominor.yy0 = yymsp[-1].minor.yy0;
   Tcl_SetObjResult(parser->ip,yygotominor.yy0);
   Tcl_DecrRefCount(yymsp[-1].minor.yy0);
   yy_destructor(18,&yymsp[0].minor);
}
#line 709 "legram.c"
        break;
      case 1:
#line 112 "legram.y"
{ EV2(yygotominor.yy0,plus,yymsp[-2].minor.yy0,yymsp[0].minor.yy0);   yy_destructor(10,&yymsp[-1].minor);
}
#line 715 "legram.c"
        break;
      case 2:
#line 113 "legram.y"
{ EV2(yygotominor.yy0,minus,yymsp[-2].minor.yy0,yymsp[0].minor.yy0);   yy_destructor(11,&yymsp[-1].minor);
}
#line 721 "legram.c"
        break;
      case 3:
#line 114 "legram.y"
{ EV2(yygotominor.yy0,power,yymsp[-2].minor.yy0,yymsp[0].minor.yy0);   yy_destructor(15,&yymsp[-1].minor);
}
#line 727 "legram.c"
        break;
      case 4:
#line 115 "legram.y"
{ EV1(yygotominor.yy0,negate,yymsp[0].minor.yy0);   yy_destructor(11,&yymsp[-1].minor);
}
#line 733 "legram.c"
        break;
      case 5:
#line 116 "legram.y"
{ EV2(yygotominor.yy0,times,yymsp[-2].minor.yy0,yymsp[0].minor.yy0);   yy_destructor(12,&yymsp[-1].minor);
}
#line 739 "legram.c"
        break;
      case 6:
#line 117 "legram.y"
{ EV2(yygotominor.yy0,divide,yymsp[-2].minor.yy0,yymsp[0].minor.yy0);   yy_destructor(13,&yymsp[-1].minor);
}
#line 745 "legram.c"
        break;
      case 7:
#line 118 "legram.y"
{ EV2(yygotominor.yy0,modulo,yymsp[-2].minor.yy0,yymsp[0].minor.yy0);   yy_destructor(14,&yymsp[-1].minor);
}
#line 751 "legram.c"
        break;
      case 8:
#line 119 "legram.y"
{ EV2(yygotominor.yy0,and,yymsp[-2].minor.yy0,yymsp[0].minor.yy0);   yy_destructor(1,&yymsp[-1].minor);
}
#line 757 "legram.c"
        break;
      case 9:
#line 120 "legram.y"
{ EV2(yygotominor.yy0,or,yymsp[-2].minor.yy0,yymsp[0].minor.yy0);   yy_destructor(2,&yymsp[-1].minor);
}
#line 763 "legram.c"
        break;
      case 10:
#line 121 "legram.y"
{ EV2(yygotominor.yy0,xor,yymsp[-2].minor.yy0,yymsp[0].minor.yy0);   yy_destructor(3,&yymsp[-1].minor);
}
#line 769 "legram.c"
        break;
      case 11:
#line 122 "legram.y"
{ 
   yygotominor.yy0 = yymsp[-1].minor.yy0; yymsp[-1].minor.yy0 = NULL; 
   CheckForError(yygotominor.yy0);
  yy_destructor(19,&yymsp[-2].minor);
  yy_destructor(20,&yymsp[0].minor);
}
#line 779 "legram.c"
        break;
      case 12:
#line 126 "legram.y"
{ EV2(yygotominor.yy0,times,yymsp[-1].minor.yy0,yymsp[0].minor.yy0); }
#line 784 "legram.c"
        break;
      case 13:
#line 127 "legram.y"
{ yygotominor.yy0 = yymsp[0].minor.yy0; yymsp[0].minor.yy0 = NULL; }
#line 789 "legram.c"
        break;
      case 14:
#line 128 "legram.y"
{ EV1(yygotominor.yy0,value,yymsp[0].minor.yy0); }
#line 794 "legram.c"
        break;
      case 15:
#line 129 "legram.y"
{
   yygotominor.yy0 = yymsp[-2].minor.yy0; 
   Tcl_ListObjAppendElement(parser->ip,yygotominor.yy0,yymsp[0].minor.yy0); 
   Tcl_DecrRefCount(yymsp[0].minor.yy0);
   // CheckForError(yygotominor.yy0); <-- can't happen
  yy_destructor(22,&yymsp[-1].minor);
}
#line 805 "legram.c"
        break;
      case 16:
#line 135 "legram.y"
{
   yygotominor.yy0 = Tcl_NewListObj(1,&yymsp[0].minor.yy0);
   Tcl_DecrRefCount(yymsp[0].minor.yy0);
   if (yygotominor.yy0) Tcl_IncrRefCount(yygotominor.yy0);
   CheckForError(yygotominor.yy0);
}
#line 815 "legram.c"
        break;
      case 17:
#line 141 "legram.y"
{ EV1(yygotominor.yy0,function,yymsp[0].minor.yy0); }
#line 820 "legram.c"
        break;
      case 18:
#line 142 "legram.y"
{
   yygotominor.yy0 = ApplyFunc(parser,yymsp[-3].minor.yy0,NULL,NULL,yymsp[-1].minor.yy0);
   CheckForError(yygotominor.yy0);
  yy_destructor(19,&yymsp[-2].minor);
  yy_destructor(20,&yymsp[0].minor);
}
#line 830 "legram.c"
        break;
      case 19:
#line 146 "legram.y"
{
   yygotominor.yy0 = ApplyFunc(parser,yymsp[-7].minor.yy0,yymsp[-5].minor.yy0,yymsp[-3].minor.yy0,yymsp[-1].minor.yy0);
   CheckForError(yygotominor.yy0);
  yy_destructor(15,&yymsp[-6].minor);
  yy_destructor(16,&yymsp[-4].minor);
  yy_destructor(19,&yymsp[-2].minor);
  yy_destructor(20,&yymsp[0].minor);
}
#line 842 "legram.c"
        break;
      case 20:
#line 150 "legram.y"
{
   yygotominor.yy0 = ApplyFunc(parser,yymsp[-5].minor.yy0,NULL,yymsp[-3].minor.yy0,yymsp[-1].minor.yy0);
   CheckForError(yygotominor.yy0);
  yy_destructor(16,&yymsp[-4].minor);
  yy_destructor(19,&yymsp[-2].minor);
  yy_destructor(20,&yymsp[0].minor);
}
#line 853 "legram.c"
        break;
      case 21:
#line 154 "legram.y"
{
   yygotominor.yy0 = ApplyFunc(parser,yymsp[-5].minor.yy0,yymsp[-3].minor.yy0,NULL,yymsp[-1].minor.yy0);
   CheckForError(yygotominor.yy0);
  yy_destructor(15,&yymsp[-4].minor);
  yy_destructor(19,&yymsp[-2].minor);
  yy_destructor(20,&yymsp[0].minor);
}
#line 864 "legram.c"
        break;
  };
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yypParser->yyidx -= yysize;
  if(yyexception) {
      yyact=YY_ACCEPT_ACTION;
  } else {
      yyact = yy_find_reduce_action(yymsp[-yysize].stateno,yygoto);
  }
  if(yyact < YYNSTATE){
#ifdef NDEBUG
    /* If we are not debugging and the reduce action popped at least
    ** one element off the stack, then we can push the new element back
    ** onto the stack here, and skip the stack overflow test in yy_shift().
    ** That gives a significant speed improvement. */
    if(yysize){
      yypParser->yyidx++;
      yymsp -= yysize-1;
      yymsp->stateno = yyact;
      yymsp->major = yygoto;
      yymsp->minor = yygotominor;
    }else
#endif
    {
      yy_shift(yypParser,yyact,yygoto,&yygotominor);
    }
  }else if(yyact == YYNSTATE + YYNRULE + 1){
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  LeParARG_FETCH;
#ifndef NDEBUG
  if(yyTraceFILE){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while(yypParser->yyidx>=0) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
#line 34 "legram.y"

   Tcl_SetResult(parser->ip, "Syntax error", TCL_STATIC);
   parser->failed = 1;
#line 917 "legram.c"
  LeParARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  YYMINORTYPE yyminor            /* The minor type of the error token */
){
  LeParARG_FETCH;
#define TOKEN (yyminor.yy0)
  LeParARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  LeParARG_FETCH;
#ifndef NDEBUG
  if(yyTraceFILE){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
  while(yypParser->yyidx>=0) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
#line 39 "legram.y"

   if( parser->failed ) Tcl_SetResult(parser->ip, "Exception encountered", TCL_STATIC);
#line 953 "legram.c"
  LeParARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "LeParAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void LePar(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  LeParTOKENTYPE yyminor       /* The value for the token */
  LeParARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  int yyact;            /* The parser action. */
  int yyendofinput;     /* True if we are at the end of input */
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
  yyParser *yypParser;  /* The parser */

  /* (re)initialize the parser, if necessary */
  yypParser = (yyParser*)yyp;
  if(yypParser->yyidx<0){
    /* if(yymajor==0) return; // not sure why this was here... */
    yypParser->yyidx = 0;
    yypParser->yyerrcnt = -1;
    yypParser->yystack[0].stateno = 0;
    yypParser->yystack[0].major = 0;
  }
  yyminorunion.yy0 = yyminor;
  yyendofinput = (yymajor==0);
  LeParARG_STORE;

#ifndef NDEBUG
  if(yyTraceFILE){
    fprintf(yyTraceFILE,"%sInput %s\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,yymajor);
    if(yyact<YYNSTATE){
      yy_shift(yypParser,yyact,yymajor,&yyminorunion);
      yypParser->yyerrcnt--;
      if(yyendofinput && yypParser->yyidx>=0){
        yymajor = 0;
      }else{
        yymajor = YYNOCODE;
      }
    }else if(yyact < YYNSTATE + YYNRULE){
      yy_reduce(yypParser,yyact-YYNSTATE);
    }else if(yyact == YY_ERROR_ACTION){
      int yymx;
#ifndef NDEBUG
      if(yyTraceFILE){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if(yypParser->yyerrcnt<0){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yymx = yypParser->yystack[yypParser->yyidx].major;
      if(yymx==YYERRORSYMBOL || yyerrorhit){
#ifndef NDEBUG
        if(yyTraceFILE){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yymajor,&yyminorunion);
        yymajor = YYNOCODE;
      }else{
         while(
          yypParser->yyidx >= 0 &&
          yymx != YYERRORSYMBOL &&
          (yyact = yy_find_shift_action(yypParser,YYERRORSYMBOL)) >= YYNSTATE
   ){
          yy_pop_parser_stack(yypParser);
        }
        if(yypParser->yyidx < 0 || yymajor==0){
          yy_destructor(yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
          yymajor = YYNOCODE;
        }else if(yymx!=YYERRORSYMBOL){
          YYMINORTYPE u2;
          u2.YYERRSYMDT = 0;
          yy_shift(yypParser,yyact,YYERRORSYMBOL,&u2);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if(yypParser->yyerrcnt<=0){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yymajor,&yyminorunion);
      if(yyendofinput){
        yy_parse_failed(yypParser);
      }
      yymajor = YYNOCODE;
#endif
    }else{
      yy_accept(yypParser);
      yymajor = YYNOCODE;
    }
  }while(yymajor!=YYNOCODE && yypParser->yyidx>=0);
  return;
}
