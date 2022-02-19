#ifndef TCL_SETRESULT_WRAPPED
#define TCL_SETRESULT_WRAPPED
#include <tcl.h>
#undef Tcl_SetResult
#define Tcl_SetResult(ip,res,fp) (tclStubsPtr->tcl_SetResult)((ip),(char*)(res),(fp))
#endif
