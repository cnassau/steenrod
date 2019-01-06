/*
 * OpenCL support routines
 *
 * Copyright (C) 2019-2019 Christian Nassau <nassau@nullhomotopie.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "opencl.h"
#include "CL/cl.h"
#include <string.h>

/* we use the STCL prefix (= Steenrod + Open CL) */

#define POLYNSP "::steenrod::"

TCL_DECLARE_MUTEX(eventProfilingMutex)
static Tcl_Obj *eventHistory;

static Tcl_Obj *OBJ_CL_PROFILING_COMMAND_QUEUED;
static Tcl_Obj *OBJ_CL_PROFILING_COMMAND_SUBMIT;
static Tcl_Obj *OBJ_CL_PROFILING_COMMAND_START;
static Tcl_Obj *OBJ_CL_PROFILING_COMMAND_END;
static Tcl_Obj *OBJ_CL_PROFILING_COMMAND_INFO;

static int ProfilingEnabled;

void CL_CALLBACK eventProfilingCB(cl_event event, cl_int event_command_exec_status, void *user_data) {
    Tcl_Obj *desc = (Tcl_Obj *)user_data;
    if(!ProfilingEnabled) {
        Tcl_DecrRefCount(desc);
        return;
    }
    Tcl_Obj *evtinfo = STcl_GetEventInfo(NULL,event);
    Tcl_ListObjAppendElement(NULL,evtinfo,OBJ_CL_PROFILING_COMMAND_INFO);
    Tcl_ListObjAppendElement(NULL,evtinfo,desc);
    Tcl_DecrRefCount(desc);
    Tcl_MutexLock(&eventProfilingMutex);
    if(NULL == eventHistory) eventHistory = Tcl_NewObj();
    Tcl_ListObjAppendElement(NULL,eventHistory,evtinfo);
    Tcl_MutexUnlock(&eventProfilingMutex);
}

int STcl_SetEventCB(cl_event evt, Tcl_Obj *obj) {
    Tcl_IncrRefCount(obj);
    return clSetEventCallback (evt, CL_COMPLETE, eventProfilingCB, (void*) obj);
}

int stclEventLogCmd(ClientData cd, Tcl_Interp *ip, int objc,
                       Tcl_Obj *CONST objv[]) {
    if(objc != 1) {
        Tcl_WrongNumArgs(ip, 1, objv, "");
        return TCL_ERROR;
    }
    Tcl_MutexLock(&eventProfilingMutex);
    if(NULL == eventHistory) eventHistory = Tcl_NewObj();
    Tcl_SetObjResult(ip,eventHistory);
    eventHistory = Tcl_NewObj();
    Tcl_MutexUnlock(&eventProfilingMutex);
    return TCL_OK;
}

#ifndef TCL_TSD_INIT
#   define TCL_TSD_INIT(keyPtr) \
     (ThreadSpecificData*)Tcl_GetThreadData((keyPtr),sizeof(ThreadSpecificData))
#endif

typedef struct ThreadSpecificData {
    stcl_context *ctx;
} ThreadSpecificData;

static Tcl_ThreadDataKey opencltskey;

stcl_context *GetThreadContext(void) {
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&opencltskey);
    return tsdPtr->ctx;
}

int STcl_GetContext(Tcl_Interp *ip, stcl_context **ctx) {
    stcl_context *ans = GetThreadContext();
    if(NULL == ans) {
        if(ip) Tcl_SetResult(ip,"no OpenCL context chosen", TCL_STATIC);
        return TCL_ERROR;
    }
    *ctx = ans;
    return TCL_OK;
}

stcl_context *ReplaceThreadContext(stcl_context *newctx) {
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&opencltskey);
    stcl_context *ans = tsdPtr->ctx;
    tsdPtr->ctx = newctx;
    return ans;
}

/* A cl_event can be bound to a Tcl variable (where it is stored
 * in an unset trace) */

char *STcl_EventUnsetProc(ClientData cd, Tcl_Interp *ip, const char *name1,
                          const char *name2, int flags) {
    cl_event evt = (cl_event)cd;
    clReleaseEvent(evt);
    return NULL;
}

int STcl_SetEventTrace(Tcl_Interp *ip, char *varname, cl_event evt) {
    Tcl_SetVar(ip, varname, "0", 0);
    int rc = Tcl_TraceVar(ip, varname, TCL_TRACE_UNSETS, STcl_EventUnsetProc,
                          (ClientData)evt);
    return rc;
}

cl_event STcl_GetEvent(Tcl_Interp *ip, char *varname) {
    cl_event ans = (cl_event)Tcl_VarTraceInfo(ip, varname, TCL_TRACE_UNSETS,
                                              STcl_EventUnsetProc, NULL);
    if (NULL == ans && NULL != ip) {
        Tcl_AppendResult(ip, "No cl_event associated to variable ", varname,
                         NULL);
    }
    return ans;
}

#define EVTPROFINFO(flag)                                                      \
    {                                                                          \
        rc = clGetEventProfilingInfo(e, flag, sizeof(val), &val, NULL);        \
        if (CL_SUCCESS != rc)                                                  \
            break;                                                             \
        Tcl_ListObjAppendElement(ip, ans, OBJ_ ## flag);        \
        Tcl_ListObjAppendElement(ip, ans, Tcl_NewLongObj(val));                \
    }
Tcl_Obj *STcl_GetEventInfo(Tcl_Interp *ip, cl_event e) {
    Tcl_Obj *ans = Tcl_NewListObj(0, NULL);
    cl_ulong val;
    cl_int rc = CL_SUCCESS;
    do {
        EVTPROFINFO(CL_PROFILING_COMMAND_QUEUED);
        EVTPROFINFO(CL_PROFILING_COMMAND_SUBMIT);
        EVTPROFINFO(CL_PROFILING_COMMAND_START);
        EVTPROFINFO(CL_PROFILING_COMMAND_END);
    } while (0);
    if (CL_SUCCESS == rc) {
        return ans;
    }
    Tcl_DecrRefCount(ans);
    if (ip)
        SetCLErrorCode(ip, rc);
    return NULL;
}

#define TRYREG(f)                                                              \
    {                                                                          \
        if (errcode == f) {                                                    \
            Tcl_SetResult(ip, #f, TCL_STATIC);                                 \
            return;                                                            \
        }                                                                      \
    }

void SetCLErrorCode(Tcl_Interp *ip, cl_int errcode) {
    TRYREG(CL_SUCCESS);
    TRYREG(CL_DEVICE_NOT_FOUND);
    TRYREG(CL_DEVICE_NOT_AVAILABLE);
    TRYREG(CL_COMPILER_NOT_AVAILABLE);
    TRYREG(CL_MEM_OBJECT_ALLOCATION_FAILURE);
    TRYREG(CL_OUT_OF_RESOURCES);
    TRYREG(CL_OUT_OF_HOST_MEMORY);
    TRYREG(CL_PROFILING_INFO_NOT_AVAILABLE);
    TRYREG(CL_MEM_COPY_OVERLAP);
    TRYREG(CL_IMAGE_FORMAT_MISMATCH);
    TRYREG(CL_IMAGE_FORMAT_NOT_SUPPORTED);
    TRYREG(CL_BUILD_PROGRAM_FAILURE);
    TRYREG(CL_MAP_FAILURE);
    TRYREG(CL_MISALIGNED_SUB_BUFFER_OFFSET);
    TRYREG(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
    TRYREG(CL_COMPILE_PROGRAM_FAILURE);
    TRYREG(CL_LINKER_NOT_AVAILABLE);
    TRYREG(CL_LINK_PROGRAM_FAILURE);
    TRYREG(CL_DEVICE_PARTITION_FAILED);
    TRYREG(CL_KERNEL_ARG_INFO_NOT_AVAILABLE);
    TRYREG(CL_INVALID_VALUE);
    TRYREG(CL_INVALID_DEVICE_TYPE);
    TRYREG(CL_INVALID_PLATFORM);
    TRYREG(CL_INVALID_DEVICE);
    TRYREG(CL_INVALID_CONTEXT);
    TRYREG(CL_INVALID_QUEUE_PROPERTIES);
    TRYREG(CL_INVALID_COMMAND_QUEUE);
    TRYREG(CL_INVALID_HOST_PTR);
    TRYREG(CL_INVALID_MEM_OBJECT);
    TRYREG(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
    TRYREG(CL_INVALID_IMAGE_SIZE);
    TRYREG(CL_INVALID_SAMPLER);
    TRYREG(CL_INVALID_BINARY);
    TRYREG(CL_INVALID_BUILD_OPTIONS);
    TRYREG(CL_INVALID_PROGRAM);
    TRYREG(CL_INVALID_PROGRAM_EXECUTABLE);
    TRYREG(CL_INVALID_KERNEL_NAME);
    TRYREG(CL_INVALID_KERNEL_DEFINITION);
    TRYREG(CL_INVALID_KERNEL);
    TRYREG(CL_INVALID_ARG_INDEX);
    TRYREG(CL_INVALID_ARG_VALUE);
    TRYREG(CL_INVALID_ARG_SIZE);
    TRYREG(CL_INVALID_KERNEL_ARGS);
    TRYREG(CL_INVALID_WORK_DIMENSION);
    TRYREG(CL_INVALID_WORK_GROUP_SIZE);
    TRYREG(CL_INVALID_WORK_ITEM_SIZE);
    TRYREG(CL_INVALID_GLOBAL_OFFSET);
    TRYREG(CL_INVALID_EVENT_WAIT_LIST);
    TRYREG(CL_INVALID_EVENT);
    TRYREG(CL_INVALID_OPERATION);
    TRYREG(CL_INVALID_GL_OBJECT);
    TRYREG(CL_INVALID_BUFFER_SIZE);
    TRYREG(CL_INVALID_MIP_LEVEL);
    TRYREG(CL_INVALID_GLOBAL_WORK_SIZE);
    TRYREG(CL_INVALID_PROPERTY);
    TRYREG(CL_INVALID_IMAGE_DESCRIPTOR);
    TRYREG(CL_INVALID_COMPILER_OPTIONS);
    TRYREG(CL_INVALID_LINKER_OPTIONS);
    TRYREG(CL_INVALID_DEVICE_PARTITION_COUNT);
    TRYREG(CL_INVALID_PIPE_SIZE);
    TRYREG(CL_INVALID_DEVICE_QUEUE);
    TRYREG(CL_INVALID_SPEC_ID);
    TRYREG(CL_MAX_SIZE_RESTRICTION_EXCEEDED);
    char ec[100];
    sprintf(ec, "unknown CL error code %d", errcode);
    Tcl_SetResult(ip, ec, TCL_VOLATILE);
}

static void AddDeviceInfo(int tp, cl_device_id d, Tcl_Obj *a, const char *name,
                          cl_device_info param) {
    size_t sz;
    char *ans = NULL;
    if (CL_SUCCESS == clGetDeviceInfo(d, param, 0, NULL, &sz)) {
        ans = malloc(sz + 1);
        if (CL_SUCCESS == clGetDeviceInfo(d, param, sz, ans, &sz)) {
            Tcl_ListObjAppendElement(0, a, Tcl_NewStringObj(name, -1));
            switch (tp) {
            case 0: {
                cl_uint *x = (cl_uint *)ans;
                Tcl_ListObjAppendElement(0, a, Tcl_NewIntObj(*x));
                break;
            }
            case 1: {
                cl_ulong *x = (cl_ulong *)ans;
                Tcl_ListObjAppendElement(0, a, Tcl_NewLongObj(*x));
                break;
            }
            case 2: {
                size_t *x = (size_t *)ans;
                Tcl_ListObjAppendElement(0, a, Tcl_NewLongObj(*x));
                break;
            }
            case 3: {
                cl_bool *x = (cl_bool *)ans;
                Tcl_ListObjAppendElement(
                    0, a, Tcl_NewStringObj((*x ? "yes" : "no"), -1));
                break;
            }
            case 4: {
                Tcl_ListObjAppendElement(0, a, Tcl_NewStringObj(ans, sz - 1));
                break;
            }
            case 5: {
                size_t *x = (size_t *)ans;
                Tcl_Obj *z = Tcl_NewObj();
                for(;sz>=sizeof(size_t);x++,sz-=sizeof(size_t)) {
                    Tcl_ListObjAppendElement(0,z,Tcl_NewLongObj(*x));
                }
                Tcl_ListObjAppendElement(0, a, z);
                break;
            }
            default:
                Tcl_ListObjAppendElement(
                    0, a, Tcl_NewStringObj("internal error", -1));
            }
        }
    }
    if (ans)
        free(ans);
}

#define ADDDEVINFO_INT(d, a, flag) AddDeviceInfo(0, d, a, #flag, flag)
#define ADDDEVINFO_LONG(d, a, flag) AddDeviceInfo(1, d, a, #flag, flag)
#define ADDDEVINFO_SIZET(d, a, flag) AddDeviceInfo(2, d, a, #flag, flag)
#define ADDDEVINFO_SIZETARR(d, a, flag) AddDeviceInfo(5, d, a, #flag, flag)
#define ADDDEVINFO_BOOL(d, a, flag) AddDeviceInfo(3, d, a, #flag, flag)
#define ADDDEVINFO_STR(d, a, flag) AddDeviceInfo(4, d, a, #flag, flag)

static Tcl_Obj *stclDeviceInfo(cl_device_id d) {
    Tcl_Obj *ans = Tcl_NewListObj(0, NULL);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_ADDRESS_BITS);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_MAX_CLOCK_FREQUENCY);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_MAX_COMPUTE_UNITS);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_MAX_CONSTANT_ARGS);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_MAX_READ_IMAGE_ARGS);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_MAX_SAMPLERS);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_MAX_WRITE_IMAGE_ARGS);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_MEM_BASE_ADDR_ALIGN);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_NATIVE_VECTOR_WIDTH_INT);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_PARTITION_MAX_SUB_DEVICES);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_REFERENCE_COUNT);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE);
    ADDDEVINFO_INT(d, ans, CL_DEVICE_VENDOR_ID);
    ADDDEVINFO_LONG(d, ans, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE);
    ADDDEVINFO_LONG(d, ans, CL_DEVICE_GLOBAL_MEM_SIZE);
    ADDDEVINFO_LONG(d, ans, CL_DEVICE_LOCAL_MEM_SIZE);
    ADDDEVINFO_LONG(d, ans, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE);
    ADDDEVINFO_LONG(d, ans, CL_DEVICE_MAX_MEM_ALLOC_SIZE);
    ADDDEVINFO_SIZET(d, ans, CL_DEVICE_IMAGE2D_MAX_HEIGHT);
    ADDDEVINFO_SIZET(d, ans, CL_DEVICE_IMAGE2D_MAX_WIDTH);
    ADDDEVINFO_SIZET(d, ans, CL_DEVICE_IMAGE3D_MAX_DEPTH);
    ADDDEVINFO_SIZET(d, ans, CL_DEVICE_IMAGE3D_MAX_HEIGHT);
    ADDDEVINFO_SIZET(d, ans, CL_DEVICE_IMAGE3D_MAX_WIDTH);
    ADDDEVINFO_SIZET(d, ans, CL_DEVICE_IMAGE_MAX_BUFFER_SIZE);
    ADDDEVINFO_SIZET(d, ans, CL_DEVICE_IMAGE_MAX_ARRAY_SIZE);
    ADDDEVINFO_SIZET(d, ans, CL_DEVICE_MAX_PARAMETER_SIZE);
    ADDDEVINFO_SIZET(d, ans, CL_DEVICE_MAX_WORK_GROUP_SIZE);
    ADDDEVINFO_SIZET(d, ans, CL_DEVICE_PRINTF_BUFFER_SIZE);
    ADDDEVINFO_SIZET(d, ans, CL_DEVICE_PROFILING_TIMER_RESOLUTION);
    ADDDEVINFO_SIZETARR(d, ans, CL_DEVICE_MAX_WORK_ITEM_SIZES);
    ADDDEVINFO_BOOL(d, ans, CL_DEVICE_AVAILABLE);
    ADDDEVINFO_BOOL(d, ans, CL_DEVICE_COMPILER_AVAILABLE);
    ADDDEVINFO_BOOL(d, ans, CL_DEVICE_ENDIAN_LITTLE);
    ADDDEVINFO_BOOL(d, ans, CL_DEVICE_ERROR_CORRECTION_SUPPORT);
    ADDDEVINFO_BOOL(d, ans, CL_DEVICE_HOST_UNIFIED_MEMORY);
    ADDDEVINFO_BOOL(d, ans, CL_DEVICE_IMAGE_SUPPORT);
    ADDDEVINFO_BOOL(d, ans, CL_DEVICE_LINKER_AVAILABLE);
    ADDDEVINFO_BOOL(d, ans, CL_DEVICE_PREFERRED_INTEROP_USER_SYNC);
    ADDDEVINFO_STR(d, ans, CL_DEVICE_BUILT_IN_KERNELS);
    ADDDEVINFO_STR(d, ans, CL_DEVICE_EXTENSIONS);
    ADDDEVINFO_STR(d, ans, CL_DEVICE_NAME);
    ADDDEVINFO_STR(d, ans, CL_DEVICE_OPENCL_C_VERSION);
    ADDDEVINFO_STR(d, ans, CL_DEVICE_PROFILE);
    ADDDEVINFO_STR(d, ans, CL_DEVICE_VENDOR);
    ADDDEVINFO_STR(d, ans, CL_DEVICE_VERSION);
    ADDDEVINFO_STR(d, ans, CL_DRIVER_VERSION);
    return ans;
}

static void AddPlatformInfo(cl_platform_id pid, Tcl_Obj *ans, const char *name,
                            cl_platform_info flag) {
    size_t s = 0;
    int vs = -1;
    char *val = NULL;
    const char *errstr = "internal error";
    Tcl_ListObjAppendElement(0, ans, Tcl_NewStringObj(name, -1));
    do {
        if (CL_SUCCESS != clGetPlatformInfo(pid, flag, 0, NULL, &s))
            break;
        val = malloc(s + 1);
        if (NULL == val)
            break;
        if (CL_SUCCESS != clGetPlatformInfo(pid, flag, s + 1, val, &s))
            break;
        errstr = NULL;
        vs = s - 1;
    } while (0);
    Tcl_ListObjAppendElement(0, ans,
                             Tcl_NewStringObj((errstr ? errstr : val), vs));
    if (val)
        free(val);
}

#define ADDPLATFORMINFO(p, obj, name) AddPlatformInfo(p, obj, #name, name)

static Tcl_Obj *stclPlatformInfo(cl_platform_id p, int withdevices) {
    Tcl_Obj *ans = Tcl_NewListObj(0, NULL);
    ADDPLATFORMINFO(p, ans, CL_PLATFORM_PROFILE);
    ADDPLATFORMINFO(p, ans, CL_PLATFORM_VERSION);
    ADDPLATFORMINFO(p, ans, CL_PLATFORM_NAME);
    ADDPLATFORMINFO(p, ans, CL_PLATFORM_VENDOR);
    ADDPLATFORMINFO(p, ans, CL_PLATFORM_EXTENSIONS);
    if (withdevices) {
        Tcl_ListObjAppendElement(0, ans, Tcl_NewStringObj("devices", -1));
        Tcl_Obj *dev = Tcl_NewListObj(0, NULL);
        cl_uint numdevices;
        if (CL_SUCCESS ==
            clGetDeviceIDs(p, CL_DEVICE_TYPE_ALL, 0, NULL, &numdevices)) {
            cl_device_id *did = malloc((sizeof(cl_device_id) * numdevices));
            if (did &&
                CL_SUCCESS == clGetDeviceIDs(p, CL_DEVICE_TYPE_ALL, numdevices,
                                             did, &numdevices)) {
                for (int i = 0; i < numdevices; i++) {
                    Tcl_ListObjAppendElement(0, dev,
                                             Tcl_NewLongObj((long)did[i]));
                    Tcl_ListObjAppendElement(0, dev, stclDeviceInfo(did[i]));
                }
            }
            if (did)
                free(did);
        }
        Tcl_ListObjAppendElement(0, ans, dev);
    }
    return ans;
}

typedef enum { MEM_INFO, MEM_SIZE, MEM_DISPOSE } memcmdcode;

static CONST char *memcmdnames[] = {"info", "size", "dispose", (char *)NULL};

static memcmdcode memcmdmap[] = {MEM_INFO, MEM_SIZE, MEM_DISPOSE};

int stclMemObjCommandInstanceCmd(ClientData cd, Tcl_Interp *ip, int objc,
                                 Tcl_Obj *CONST objv[]) {
    cl_mem p = (cl_mem)cd;

    int result, index;
    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result =
        Tcl_GetIndexFromObj(ip, objv[1], memcmdnames, "subcommand", 0, &index);
    if (result != TCL_OK)
        return result;

    switch (memcmdmap[index]) {
    case MEM_INFO: {

        return TCL_OK;
    }
    case MEM_SIZE: {
        size_t sz;
        clGetMemObjectInfo(p,CL_MEM_SIZE,sizeof(sz),&sz,NULL);
        Tcl_SetObjResult(ip,Tcl_NewLongObj(sz));
        return TCL_OK;
    }
    case MEM_DISPOSE: {
        return Tcl_DeleteCommand(ip,Tcl_GetString(objv[0]));
    }
    }
    return TCL_ERROR;
}

void stclMemObjInstanceDestructor(ClientData cd) {
    cl_mem mem = (cl_mem)cd;
    clReleaseMemObject(mem);
}

int STcl_CreateMemObj(Tcl_Interp *ip, Tcl_Obj *obj, cl_mem mem) {
    // FIXME: optimize for reuse of MemObj commands
    if (NULL == Tcl_CreateObjCommand(ip, Tcl_GetString(obj),
                                     stclMemObjCommandInstanceCmd, mem,
                                     stclMemObjInstanceDestructor)) {
        return TCL_ERROR;
    }
    return TCL_OK;
}

static const char *memflagnames[] = {
    "CL_MEM_READ_WRITE",      "CL_MEM_WRITE_ONLY",
    "CL_MEM_READ_ONLY",       "CL_MEM_USE_HOST_PTR",
    "CL_MEM_ALLOC_HOST_PTR",  "CL_MEM_COPY_HOST_PTR",
    "CL_MEM_HOST_WRITE_ONLY", "CL_MEM_HOST_READ_ONLY",
    "CL_MEM_HOST_NO_ACCESS",  NULL};

static int memflagmap[] = {
    CL_MEM_READ_WRITE,      CL_MEM_WRITE_ONLY,     CL_MEM_READ_ONLY,
    CL_MEM_USE_HOST_PTR,    CL_MEM_ALLOC_HOST_PTR, CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_WRITE_ONLY, CL_MEM_HOST_READ_ONLY, CL_MEM_HOST_NO_ACCESS};

int GetMemFlagFromTclObj(Tcl_Interp *ip, Tcl_Obj *obj, int *ans) {
    *ans = 0;
    int oc;
    Tcl_Obj **ov;
    if (TCL_OK != Tcl_ListObjGetElements(ip, obj, &oc, &ov))
        return TCL_ERROR;
    for (int i = 0; i < oc; i++) {
        int idx;
        if (TCL_OK != Tcl_GetIndexFromObj(ip, ov[i], memflagnames,
                                          "CL memory flag", 0, &idx))
            return TCL_ERROR;
        *ans |= memflagmap[idx];
    }
    return TCL_OK;
}

typedef enum { KERN_WGINFO, KERN_SETARG } kerncmdcode;

static CONST char *kerncmdnames[] = {"workgroupinfo", "setarg", (char *)NULL};

static kerncmdcode kerncmdmap[] = {KERN_WGINFO, KERN_SETARG};

typedef enum { KA_INT, KA_BUFFER, KA_LOCAL } kacode;

static CONST char *kanames[] = {"integer", "buffer", "local", (char *)NULL};

static kacode kamap[] = {KA_INT, KA_BUFFER, KA_LOCAL};

void AddKerWGInfo(cl_kernel k, cl_device_id d, Tcl_Interp *ip, const char *name,
                  cl_kernel_work_group_info flag, int tp) {
    Tcl_AppendElement(ip, name);
    size_t sz[3];
    cl_ulong ui;
    char num[215];
    switch (tp) {
    case 1: {
        ui = 0;
        clGetKernelWorkGroupInfo(k, d, flag, sizeof(ui), &ui, NULL);
        unsigned long x = ui;
        sprintf(num, "%ld", x);
        Tcl_AppendElement(ip, num);
        break;
    }
    case 2: {
        sz[0] = 0;
        clGetKernelWorkGroupInfo(k, d, flag, sizeof(sz[0]), sz, NULL);
        sprintf(num, "%ld", sz[0]);
        Tcl_AppendElement(ip, num);
        break;
    }
    case 3: {
        sz[0] = sz[1] = sz[2] = 0;
        clGetKernelWorkGroupInfo(k, d, flag, sizeof(sz), sz, NULL);
        sprintf(num, "%ld %ld %ld", sz[0], sz[1], sz[2]);
        Tcl_AppendElement(ip, num);
        break;
    }
    }
}

int stclKernelInstanceCmd(ClientData cd, Tcl_Interp *ip, int objc,
                          Tcl_Obj *CONST objv[]) {
    stcl_kernel *p = (stcl_kernel *)cd;

    int result, index;
    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result =
        Tcl_GetIndexFromObj(ip, objv[1], kerncmdnames, "subcommand", 0, &index);
    if (result != TCL_OK)
        return result;

    switch (kerncmdmap[index]) {
    case KERN_WGINFO: {
#define ADDKERWGINFO(flag, tp)                                                 \
    AddKerWGInfo(p->ker, p->dev, ip, #flag, flag, tp);
#if 0
        ADDKERWGINFO(CL_KERNEL_GLOBAL_WORK_SIZE, 3);
#endif
        ADDKERWGINFO(CL_KERNEL_WORK_GROUP_SIZE, 2);
        ADDKERWGINFO(CL_KERNEL_COMPILE_WORK_GROUP_SIZE, 3);
        ADDKERWGINFO(CL_KERNEL_LOCAL_MEM_SIZE, 1);
        ADDKERWGINFO(CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, 2);
        ADDKERWGINFO(CL_KERNEL_PRIVATE_MEM_SIZE, 1);
        return TCL_OK;
    }
    case KERN_SETARG: {
        if (objc != 5) {
            Tcl_WrongNumArgs(ip, 2, objv, "argnum argtype argval");
            return TCL_ERROR;
        }
        int argnum, rc;
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[2], &argnum))
            return TCL_ERROR;
        result = Tcl_GetIndexFromObj(ip, objv[3], kanames, "argument type", 0,
                                     &index);
        if (result != TCL_OK)
            return result;
        switch (kamap[index]) {
        case KA_INT: {
            int val;
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &val))
                return TCL_ERROR;
            rc = clSetKernelArg(p->ker, argnum, sizeof(val), &val);
            if (CL_SUCCESS != rc) {
                SetCLErrorCode(ip, rc);
                return TCL_ERROR;
            }
            return TCL_OK;
        }
        case KA_BUFFER: {
            cl_mem mem;
            if (TCL_OK != STcl_GetMemObjFromObj(ip, objv[4], &mem))
                return TCL_ERROR;
            rc = clSetKernelArg(p->ker, argnum, sizeof(mem), &mem);
            if (CL_SUCCESS != rc) {
                SetCLErrorCode(ip, rc);
                return TCL_ERROR;
            }
            return TCL_OK;
        }
        case KA_LOCAL: {
            int val;
            if (TCL_OK != Tcl_GetIntFromObj(ip, objv[4], &val))
                return TCL_ERROR;
            rc = clSetKernelArg(p->ker, argnum, val, NULL);
            if (CL_SUCCESS != rc) {
                SetCLErrorCode(ip, rc);
                return TCL_ERROR;
            }
            return TCL_OK;
        }
        }
        return TCL_ERROR;
    }
    default:
        break;
    }
    return TCL_ERROR;
}

void stclKernelInstanceDestructor(ClientData cd) {
    stcl_kernel *p = (stcl_kernel *)cd;
    clReleaseKernel(p->ker);
    clReleaseDevice(p->dev);
    free(p);
}

typedef enum { PROG_LIST, PROG_KERNEL } progcmdcode;

static CONST char *progcmdnames[] = {"list", "kernel", (char *)NULL};

static progcmdcode progcmdmap[] = {PROG_LIST, PROG_KERNEL};

int stclProgramInstanceCmd(ClientData cd, Tcl_Interp *ip, int objc,
                           Tcl_Obj *CONST objv[]) {
    cl_program p = (cl_program)cd;

    int result, index;
    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result =
        Tcl_GetIndexFromObj(ip, objv[1], progcmdnames, "subcommand", 0, &index);
    if (result != TCL_OK)
        return result;

    switch (progcmdmap[index]) {
    case PROG_LIST: {
        size_t siz;
        if (CL_SUCCESS ==
            clGetProgramInfo(p, CL_PROGRAM_KERNEL_NAMES, 0, NULL, &siz)) {
            char *ans = malloc(siz + 1);
            if (ans &&
                CL_SUCCESS == clGetProgramInfo(p, CL_PROGRAM_KERNEL_NAMES, siz,
                                               ans, NULL)) {
                ans[siz] = 0;
                for (char *a = ans; *a; a++)
                    if (';' == *a)
                        *a = ' ';
                Tcl_SetResult(ip, ans, TCL_VOLATILE);
                free(ans);
                return TCL_OK;
            }
            if (ans)
                free(ans);
        }
        Tcl_SetResult(ip, "could not get list of kernels", TCL_STATIC);
        return TCL_ERROR;
    }
    case PROG_KERNEL: {
        if (objc < 4 || objc > 4) {
            Tcl_WrongNumArgs(ip, 2, objv, "objname kernelname");
            return TCL_ERROR;
        }
        cl_int errcode;
        cl_kernel k = clCreateKernel(p, Tcl_GetString(objv[3]), &errcode);
        if (CL_SUCCESS == errcode) {
            stcl_kernel *ker = malloc(sizeof(stcl_kernel));
            ker->ker = k;
            clGetProgramInfo(p, CL_PROGRAM_DEVICES, sizeof(cl_device_id),
                             &(ker->dev), NULL);
            clRetainKernel(ker->ker);
            clRetainDevice(ker->dev);
            Tcl_CreateObjCommand(ip, Tcl_GetString(objv[2]),
                                 stclKernelInstanceCmd, (ClientData)ker,
                                 stclKernelInstanceDestructor);
            return TCL_OK;
        }
        SetCLErrorCode(ip, errcode);
        return TCL_ERROR;
    }
    }
    return TCL_ERROR;
}

void stclProgramInstanceDestructor(ClientData cd) {
    cl_program p = (cl_program)cd;
    clReleaseProgram(p);
}

#define stringify(x) #x

cl_command_queue GetOrCreateCommandQueue(Tcl_Interp *ip, stcl_context *ctx,
                                         int queue) {
    if (queue < 0 || queue >= MAXQUEUE) {
        Tcl_SetResult(ip,
                      "queue number must be between 0 and " stringify(MAXQUEUE),
                      TCL_STATIC);
        return 0;
    }
    cl_command_queue q = ctx->queue[queue];
    if (NULL == q) {
        cl_int errcode;
#if 1
        q = clCreateCommandQueue(
            ctx->ctx, ctx->did,
            ctx->create_queues_with_profiling ? CL_QUEUE_PROFILING_ENABLE : 0,
            &errcode);
#else
        /* this code requires OpenCL 2.0 (cores with oclgrind) */
        cl_queue_properties props[3];
        props[0] = CL_QUEUE_PROPERTIES;
        props[1] = ctx->create_queues_with_profiling ? CL_QUEUE_PROFILING_ENABLE : 0;
        props[2] = 0;
        q = clCreateCommandQueueWithProperties(
            ctx->ctx, ctx->did,
            props,
            &errcode);
#endif
        ctx->queue[queue] = q;
        if (NULL == q)
            SetCLErrorCode(ip, errcode);
    }
    return q;
}

static int makeWaitList(Tcl_Interp *ip, Tcl_Obj *o, int *numwait,
                        cl_event **evtlist) {
    Tcl_Obj **obj;
    *evtlist = NULL;
    if (TCL_OK != Tcl_ListObjGetElements(ip, o, numwait, &obj))
        return TCL_ERROR;
    if (*numwait) {
        cl_event *el = calloc(*numwait, sizeof(cl_event));
        for (int i = *numwait; i--;) {
            cl_event e = STcl_GetEvent(ip, Tcl_GetString(obj[i]));
            if (NULL == e) {
                free(el);
                return TCL_ERROR;
            }
            el[i] = e;
        }
        *evtlist = el;
    }
    return TCL_OK;
}

typedef enum {
    CTX_PLATFORM,
    CTX_DEVICE,
    CTX_PROGRAM,
    CTX_PROFILING,
    CTX_ENQUEUE_NDR,
    CTX_ENQUEUE_TASK,
    CTX_ENQUEUE_BARRIER,
    CTX_FINISH,
    CTX_EVENTINFO,
    CTX_SETCONTEXT
} ctxcmdcode;

static CONST char *ctxcmdnames[] = {
    "platform", "device",  "program", "profiling", "enqndr",
    "enqtask",  "barrier", "finish",  "eventinfo", "setcontext", (char *)NULL};

static ctxcmdcode ctxcmdmap[] = {
    CTX_PLATFORM,        CTX_DEVICE,      CTX_PROGRAM,
    CTX_PROFILING,       CTX_ENQUEUE_NDR, CTX_ENQUEUE_TASK,
    CTX_ENQUEUE_BARRIER, CTX_FINISH,      CTX_EVENTINFO,
    CTX_SETCONTEXT};

int stclContextInstanceCmd(ClientData cd, Tcl_Interp *ip, int objc,
                           Tcl_Obj *CONST objv[]) {
    stcl_context *dev = (stcl_context *)cd;
    int result, index;
    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result =
        Tcl_GetIndexFromObj(ip, objv[1], ctxcmdnames, "subcommand", 0, &index);
    if (result != TCL_OK)
        return result;

    switch (ctxcmdmap[index]) {
    case CTX_PLATFORM: {
        Tcl_SetObjResult(ip,
                         stclPlatformInfo(dev->pid, 0 /* without devices */));
        return TCL_OK;
    }
    case CTX_DEVICE: {
        Tcl_SetObjResult(ip, stclDeviceInfo(dev->did));
        return TCL_OK;
    }
    case CTX_PROFILING: {
        if (objc == 2) {
            Tcl_SetObjResult(
                ip, Tcl_NewBooleanObj(dev->create_queues_with_profiling));
            return TCL_OK;
        }
        if (objc > 2) {
            int bool;
            if (TCL_OK != Tcl_GetBooleanFromObj(ip, objv[2], &bool)) {
                return TCL_ERROR;
            }
            dev->create_queues_with_profiling = bool;
            return TCL_OK;
        }
        break;
    }
    case CTX_ENQUEUE_BARRIER: {
        if (objc < 3 || objc > 6) {
            Tcl_WrongNumArgs(ip, 2, objv, "queueNumber ?waitvars? ?eventvar? ?eventdesc?");
            return TCL_ERROR;
        }
        int queue;
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[2], &queue))
            return TCL_ERROR;
        cl_command_queue q = GetOrCreateCommandQueue(ip, dev, queue);
        if (q == NULL)
            return TCL_ERROR;
        int numwait = 0;
        cl_event *evtlist = NULL, evt, *evtptr = (5 <= objc) ? &evt : NULL;
        if (objc >= 4 &&
            TCL_OK != makeWaitList(ip, objv[3], &numwait, &evtlist))
            return TCL_ERROR;
        clEnqueueBarrierWithWaitList(q, numwait, evtlist, evtptr);
        if (evtptr) {
            STcl_SetEventTrace(ip, Tcl_GetString(objv[4]), evt);
            if(objc >= 6) STcl_SetEventCB(evt,objv[5]);
        }
        if (evtlist)
            free(evtlist);
        return TCL_OK;
    }
    case CTX_FINISH: {
        if (objc != 3) {
            Tcl_WrongNumArgs(ip, 2, objv, "queueNumber");
            return TCL_ERROR;
        }
        int queue;
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[2], &queue))
            return TCL_ERROR;
        cl_command_queue q = GetOrCreateCommandQueue(ip, dev, queue);
        if (q == NULL)
            return TCL_ERROR;
        clFinish(q);
        return TCL_OK;
    }
    case CTX_ENQUEUE_NDR: {
        if (objc < 7 || objc > 10) {
            Tcl_WrongNumArgs(
                ip, 2, objv,
                "queueNumber kernel global_work_offset global_work_size "
                "local_work_size ?waitvars? ?eventvar? ?eventdesc?");
            return TCL_ERROR;
        }
        int queue;
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[2], &queue))
            return TCL_ERROR;
        size_t globaloffset[3], globalsize[3], localsize[3];
        Tcl_Obj **listptr;
        int i, j, lcnt1, lcnt2, lcnt3;
        if (TCL_OK != Tcl_ListObjGetElements(ip, objv[4], &lcnt1, &listptr))
            return TCL_ERROR;
        for (i = 0; i < 3 && i < lcnt1; i++)
            if (TCL_OK != Tcl_GetIntFromObj(ip, listptr[i], &j))
                return TCL_ERROR;
            else
                globaloffset[i] = j;

        if (TCL_OK != Tcl_ListObjGetElements(ip, objv[5], &lcnt2, &listptr))
            return TCL_ERROR;
        for (i = 0; i < 3 && i < lcnt2; i++)
            if (TCL_OK != Tcl_GetIntFromObj(ip, listptr[i], &j))
                return TCL_ERROR;
            else
                globalsize[i] = j;

        if (TCL_OK != Tcl_ListObjGetElements(ip, objv[6], &lcnt3, &listptr))
            return TCL_ERROR;
        for (i = 0; i < 3 && i < lcnt3; i++)
            if (TCL_OK != Tcl_GetIntFromObj(ip, listptr[i], &j))
                return TCL_ERROR;
            else
                localsize[i] = j;
        if (lcnt1 == 0) {
            lcnt1 = lcnt2;
            globaloffset[0] = globaloffset[1] = globaloffset[2] = 0;
        }
        if (lcnt1 != lcnt2 || (lcnt3 && (lcnt1 != lcnt3))) {
            Tcl_SetResult(ip, "inconsistent work dimensions", TCL_STATIC);
            return TCL_ERROR;
        }
        if (lcnt1 > 3) {
            Tcl_SetResult(ip, "work dimensions bigger than 3", TCL_STATIC);
            return TCL_ERROR;
        }
        if (lcnt3) {
            for (i = 0; i < lcnt3; i++) {
                if (0 == localsize[i]) {
                    Tcl_SetResult(ip,
                                  "local work size members must not be zero",
                                  TCL_STATIC);
                    return TCL_ERROR;
                }
                if (0 != (globalsize[i] % localsize[i])) {
                    Tcl_SetResult(
                        ip,
                        "global work size must be multiple of local work size",
                        TCL_STATIC);
                    return TCL_ERROR;
                }
            }
        }
        stcl_kernel *ker;
        if (TCL_OK != STcl_GetKernelFromObj(ip, objv[3], &ker))
            return TCL_ERROR;

        cl_command_queue q = GetOrCreateCommandQueue(ip, dev, queue);
        if (q == NULL)
            return TCL_ERROR;

        int numwait = 0;
        cl_event *evtlist = NULL, evt, *evtptr = (9 <= objc) ? &evt : NULL;
        if (objc >= 8 &&
            TCL_OK != makeWaitList(ip, objv[7], &numwait, &evtlist))
            return TCL_ERROR;
        cl_int rc = clEnqueueNDRangeKernel(q, ker->ker, lcnt1, globaloffset,
                                           globalsize, lcnt3 ? localsize : NULL,
                                           numwait, evtlist, evtptr);
        if (evtlist)
            free(evtlist);
        if (CL_SUCCESS == rc) {
            if (evtptr) {
                STcl_SetEventTrace(ip, Tcl_GetString(objv[8]), evt);
                if(objc >= 10) STcl_SetEventCB(evt,objv[9]);
            }
            return TCL_OK;
        }
        SetCLErrorCode(ip, rc);
        return TCL_ERROR;
    }
    case CTX_ENQUEUE_TASK: {
        if (objc < 4 || objc > 7) {
            Tcl_WrongNumArgs(ip, 2, objv,
                             "queueNumber kernel ?waitvars? ?eventvar? ?eventdesc?");
            return TCL_ERROR;
        }
        int queue;
        if (TCL_OK != Tcl_GetIntFromObj(ip, objv[2], &queue))
            return TCL_ERROR;
        stcl_kernel *ker;
        if (TCL_OK != STcl_GetKernelFromObj(ip, objv[3], &ker))
            return TCL_ERROR;
        cl_command_queue q = GetOrCreateCommandQueue(ip, dev, queue);
        if (q == NULL)
            return TCL_ERROR;
        int numwait = 0;
        cl_event *evtlist = NULL, evt, *evtptr = (6 <= objc) ? &evt : NULL;
        if (objc >= 5 &&
            TCL_OK != makeWaitList(ip, objv[4], &numwait, &evtlist))
            return TCL_ERROR;
#if 0
        cl_int rc = clEnqueueTask(q, ker->ker, numwait, evtlist, evtptr);
#else
        size_t wd = 1;
        cl_int rc = clEnqueueNDRangeKernel(q,ker->ker,1,NULL,&wd,&wd,numwait,evtlist,evtptr);
#endif
        if (evtlist)
            free(evtlist);
        if (CL_SUCCESS == rc) {
            if (evtptr) {
                STcl_SetEventTrace(ip, Tcl_GetString(objv[5]), evt);
                if(objc >= 7) STcl_SetEventCB(evt,objv[6]);
            }
            return TCL_OK;
        }
        SetCLErrorCode(ip, rc);
        return TCL_ERROR;
    }
    case CTX_EVENTINFO: {
        if (objc != 3) {
            Tcl_WrongNumArgs(ip, 2, objv, "eventvar");
            return TCL_ERROR;
        }
        cl_event e = STcl_GetEvent(ip, Tcl_GetString(objv[2]));
        if (NULL == e)
            return TCL_ERROR;
        Tcl_Obj *ans = STcl_GetEventInfo(ip,e);
        if(ans) {
            Tcl_SetObjResult(ip,ans);
            return TCL_OK;
        }
        return TCL_ERROR;
    }
    case CTX_PROGRAM: {
        cl_int retcode;
        const char *code[1];
        size_t lens[1];
        cl_device_id devices[1];
        Tcl_Obj *options = NULL;
        if (objc < 4 || objc > 5) {
            Tcl_WrongNumArgs(ip, 2, objv, "objname code ?options?");
            return TCL_ERROR;
        }
        if (objc == 5)
            options = objv[4];
        code[0] = Tcl_GetString(objv[3]);
        lens[0] = strlen(code[0]);
        devices[0] = dev->did;
        cl_program prog =
            clCreateProgramWithSource(dev->ctx, 1, code, lens, &retcode);
        if (CL_SUCCESS == retcode) {
            retcode = clBuildProgram(prog, 1, devices,
                                     options ? Tcl_GetString(options) : NULL,
                                     NULL, NULL);
            cl_int bstat = CL_BUILD_ERROR;
            if (CL_SUCCESS == retcode)
                clGetProgramBuildInfo(prog, dev->did, CL_PROGRAM_BUILD_STATUS,
                                      sizeof(cl_int), &bstat, NULL);
            if (CL_BUILD_SUCCESS == bstat &&
                CL_SUCCESS == clRetainProgram(prog)) {
                Tcl_CreateObjCommand(ip, Tcl_GetString(objv[2]),
                                     stclProgramInstanceCmd, (ClientData)prog,
                                     stclProgramInstanceDestructor);
                return TCL_OK;
            }
            switch (bstat) {
            case CL_BUILD_NONE:
                Tcl_AppendResult(ip, "CL_BUILD_NONE", NULL);
                break;
            case CL_BUILD_ERROR:
                Tcl_AppendResult(ip, "CL_BUILD_ERROR", NULL);
                break;
            case CL_BUILD_SUCCESS:
                Tcl_AppendResult(ip, "CL_BUILD_SUCCESS", NULL);
                break;
            default:
                Tcl_AppendResult(ip, "CL_PROGRAM_BUILD_STATUS not recognized",
                                 NULL);
                break;
            }
            size_t logsize;
            if (CL_SUCCESS == clGetProgramBuildInfo(prog, dev->did,
                                                    CL_PROGRAM_BUILD_LOG, 0,
                                                    NULL, &logsize)) {
                char *log = malloc(logsize + 1);
                if (log && CL_SUCCESS ==
                               clGetProgramBuildInfo(prog, dev->did,
                                                     CL_PROGRAM_BUILD_LOG,
                                                     logsize + 1, log, NULL)) {
                    if (log && *log)
                        Tcl_AppendResult(ip, ": ", log, NULL);
                    free(log);
                    return TCL_ERROR;
                }
                if (log)
                    free(log);
            }
            clReleaseProgram(prog);
        }
        SetCLErrorCode(ip, retcode);
    }
    case CTX_SETCONTEXT: {
        ReplaceThreadContext(dev);
        return TCL_OK;
    }
    }
    return TCL_ERROR;
}

void stclContextInstanceDestructor(ClientData cd) {
    int i;
    stcl_context *dev = (stcl_context *)cd;
    if (dev->ctx)
        clReleaseContext(dev->ctx);
    for (i = 0; i < MAXQUEUE; i++)
        if (dev->queue[i] != NULL)
            clReleaseCommandQueue(dev->queue[i]);
    free(cd);
}

void clnotifycb(const char *errinfo, const void *private_info, size_t cb,
                void *user_data) {
    if (NULL == errinfo)
        errinfo = "(not provided)";
    fprintf(stderr, "OpenCL error: %s\n", errinfo);
}

typedef enum { PL_LIST, PL_CONTEXT } plcmdcode;

static CONST char *plcmdnames[] = {"list", "context", (char *)NULL};

static plcmdcode plcmdmap[] = {PL_LIST, PL_CONTEXT};

int stclPlatformCmd(ClientData cd, Tcl_Interp *ip, int objc,
                    Tcl_Obj *CONST objv[]) {
    int result, index;

    if (objc < 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    result =
        Tcl_GetIndexFromObj(ip, objv[1], plcmdnames, "subcommand", 0, &index);
    if (result != TCL_OK)
        return result;

    switch (plcmdmap[index]) {
    case PL_LIST: {
        cl_uint numentries;
        cl_platform_id dummy; // oclgrind needs a non-zero &dummy pointer
        if (CL_SUCCESS != clGetPlatformIDs(1, &dummy, &numentries)) {
            Tcl_SetResult(ip, "cannot get number of platforms", TCL_STATIC);
            return TCL_ERROR;
        }
        cl_platform_id *ids = malloc(sizeof(cl_platform_id) * numentries);
        if (CL_SUCCESS != clGetPlatformIDs(numentries, ids, NULL)) {
            Tcl_SetResult(ip, "cannot retrieve list of available platforms",
                          TCL_STATIC);
            return TCL_ERROR;
        }
        Tcl_Obj *ans = Tcl_NewListObj(0, NULL);
        for (int i = 0; i < numentries; i++) {
            Tcl_ListObjAppendElement(ip, ans, Tcl_NewLongObj((long)ids[i]));
            Tcl_ListObjAppendElement(
                ip, ans, stclPlatformInfo(ids[i], 1 /* with devices */));
        }
        Tcl_SetObjResult(ip, ans);
        break;
    }
    case PL_CONTEXT: {
        long pid, did;
        if (objc != 5 || (TCL_OK != Tcl_GetLongFromObj(ip, objv[3], &pid)) ||
            (TCL_OK != Tcl_GetLongFromObj(ip, objv[4], &did))) {
            Tcl_WrongNumArgs(ip, 2, objv,
                             "objname cl_platform_id cl_device_id");
            return TCL_ERROR;
        }

        stcl_context *dev = calloc(1, sizeof(stcl_context));
        dev->pid = (cl_platform_id)pid;
        dev->did = (cl_device_id)did;

        cl_context_properties props[3];
        cl_device_id devs[1];
        cl_int retcode;
        props[0] = CL_CONTEXT_PLATFORM;
        props[1] = (cl_context_properties)dev->pid;
        props[2] = (cl_context_properties)NULL;
        devs[0] = dev->did;
        dev->ctx = clCreateContext(props, 1, devs, clnotifycb, NULL, &retcode);
        if (CL_SUCCESS != retcode) {
            SetCLErrorCode(ip, retcode);
            free(dev);
            return TCL_ERROR;
        }

        Tcl_CreateObjCommand(ip, Tcl_GetString(objv[2]), stclContextInstanceCmd,
                             (ClientData)dev, stclContextInstanceDestructor);
        return TCL_OK;
    }
    }

    return TCL_OK;
}

int STcl_GetKernelFromObj(Tcl_Interp *ip, Tcl_Obj *obj, stcl_kernel **kernel) {
    Tcl_CmdInfo cmdinfo;
    if (!Tcl_GetCommandInfoFromToken(Tcl_GetCommandFromObj(ip, obj),
                                     &cmdinfo)) {
        Tcl_AppendResult(ip, "command \"", Tcl_GetString(obj), "\" not found",
                         NULL);
        return TCL_ERROR;
    }
    if (cmdinfo.objProc == stclKernelInstanceCmd) {
        *kernel = (stcl_kernel *)cmdinfo.objClientData;
        return TCL_OK;
    }
    Tcl_SetResult(ip, "command is not a opencl kernel instance", TCL_STATIC);
    return TCL_ERROR;
}

int STcl_GetContextFromObj(Tcl_Interp *ip, Tcl_Obj *obj,
                           stcl_context **context) {
    Tcl_CmdInfo cmdinfo;
    if (!Tcl_GetCommandInfoFromToken(Tcl_GetCommandFromObj(ip, obj),
                                     &cmdinfo)) {
        Tcl_AppendResult(ip, "command \"", Tcl_GetString(obj), "\" not found",
                         NULL);
        return TCL_ERROR;
    }
    if (cmdinfo.objProc == stclContextInstanceCmd) {
        *context = (stcl_context *)cmdinfo.objClientData;
        return TCL_OK;
    }
    Tcl_SetResult(ip, "command is not a opencl context instance", TCL_STATIC);
    return TCL_ERROR;
}

int STcl_GetMemObjFromObj(Tcl_Interp *ip, Tcl_Obj *obj, cl_mem *memptr) {
    Tcl_CmdInfo cmdinfo;
    if (!Tcl_GetCommandInfoFromToken(Tcl_GetCommandFromObj(ip, obj),
                                     &cmdinfo)) {
        Tcl_AppendResult(ip, "command \"", Tcl_GetString(obj), "\" not found",
                         NULL);
        return TCL_ERROR;
    }
    if (cmdinfo.objProc == stclMemObjCommandInstanceCmd) {
        *memptr = (cl_mem)cmdinfo.objClientData;
        return TCL_OK;
    }
    Tcl_SetResult(ip, "command is not a opencl memory object", TCL_STATIC);
    return TCL_ERROR;
}

int stclEventMoveCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[]) {
    if (objc != 3) {
        Tcl_WrongNumArgs(ip, 1, objv, "oldvarname newvarname");
        return TCL_ERROR;
    }
    char *oldvar = Tcl_GetString(objv[1]);
    char *newvar = Tcl_GetString(objv[2]);
    cl_event evt = STcl_GetEvent(ip,oldvar);
    if(NULL == evt) return TCL_ERROR;
    clRetainEvent(evt);
    STcl_SetEventTrace(ip,newvar,evt);
    Tcl_UnsetVar(ip,oldvar,0);
    return TCL_OK;                           
}

int stclEventWaitCmd(ClientData cd, Tcl_Interp *ip, int objc,
                     Tcl_Obj *CONST objv[]) {
    if (objc != 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "eventlist");
        return TCL_ERROR;
    }
    int i, oc;
    Tcl_Obj **ov;
    if (TCL_OK != Tcl_ListObjGetElements(ip, objv[1], &oc, &ov))
        return TCL_ERROR;
    cl_event *evts = malloc(sizeof(cl_event) * oc);
    if (NULL == evts) {
        Tcl_SetResult(ip, "out of memory", TCL_STATIC);
        return TCL_ERROR;
    }
    for (i = 0; i < oc; i++) {
        evts[i] = STcl_GetEvent(ip,Tcl_GetString(ov[i]));
        if (NULL == evts[i]) {
            free(evts);
            return TCL_ERROR;
        }
    }
    i = clWaitForEvents(oc,evts);
    free(evts);
    if(i!=CL_SUCCESS) {
        SetCLErrorCode(ip,i);
        return TCL_ERROR;
    }
    return TCL_OK;
}

int stclBufferCreateCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *CONST objv[]) {
    if (objc < 3 || objc > 4) {
        Tcl_WrongNumArgs(ip, 1, objv, "bufname varname ?flags?");
        return TCL_ERROR;
    }
    stcl_context *ctx;
    if (TCL_OK != STcl_GetContext(ip, &ctx))
        return TCL_ERROR;
    int flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;
    if(objc == 4 && TCL_OK != GetMemFlagFromTclObj(ip,objv[3],&flags))
        return TCL_ERROR;
    Tcl_Obj *val = Tcl_ObjGetVar2(ip,objv[2],NULL,TCL_LEAVE_ERR_MSG);
    if(NULL == val) return TCL_ERROR;
    if(0 != (CL_MEM_USE_HOST_PTR & flags) && Tcl_IsShared(val)) {
        Tcl_SetResult(ip,"variable value is shared, cannot use CL_MEM_USE_HOST_PTR", TCL_STATIC);
        return TCL_ERROR;
    }
    int dlen;
    unsigned char *data = Tcl_GetByteArrayFromObj(val,&dlen);
    int rc;
    cl_mem clm = clCreateBuffer(ctx->ctx,flags,dlen,data,&rc);
    if(NULL == clm || rc != CL_SUCCESS) {
        SetCLErrorCode(ip,rc);
        return TCL_ERROR;
    }
    if(0 != (CL_MEM_USE_HOST_PTR & flags)) {
        // the data pointer is now no longer owned by objv[2]
        objv[2]->typePtr = NULL;
    }
    if (NULL == Tcl_ObjSetVar2(ip, objv[2], NULL, Tcl_NewObj(),
                               TCL_LEAVE_ERR_MSG) ||
        TCL_OK != STcl_CreateMemObj(ip, objv[1], clm)) {
        clReleaseMemObject(clm);
        return TCL_ERROR;
    }
    return TCL_OK;                           
}

int stclBufferValueCmd(ClientData cd, Tcl_Interp *ip, int objc,
                       Tcl_Obj *CONST objv[]) {
    if (objc != 3) {
        Tcl_WrongNumArgs(ip, 1, objv, "buffer varname");
        return TCL_ERROR;
    }
    stcl_context *ctx;
    if (TCL_OK != STcl_GetContext(ip, &ctx))
        return TCL_ERROR;
    cl_mem clm;
    if (TCL_OK != STcl_GetMemObjFromObj(ip, objv[1], &clm))
        return TCL_ERROR;
    size_t dsize;
    int rc;
    if (CL_SUCCESS != (rc = clGetMemObjectInfo(clm, CL_MEM_SIZE, sizeof(dsize),
                                               &dsize, NULL))) {
        SetCLErrorCode(ip, rc);
        return TCL_ERROR;
    }
    Tcl_Obj *ans = Tcl_NewObj();
    unsigned char *dptr = NULL;
    if (ans)
        dptr = Tcl_SetByteArrayLength(ans, dsize);
    do {
        rc = TCL_ERROR;
        cl_command_queue q = GetOrCreateCommandQueue(ip, ctx, 0);
        if (NULL == q)
            break;
        rc = clEnqueueReadBuffer(q, clm, 1 /* blocking */, 0, dsize, dptr, 0,
                                 NULL, NULL);
        if (CL_SUCCESS != rc) {
            SetCLErrorCode(ip, rc);
            Tcl_AddErrorInfo(ip, " from clEnqueueReadBuffer");
            rc = TCL_ERROR;
            break;
        }
        if(NULL == Tcl_ObjSetVar2(ip,objv[2],NULL,ans,TCL_LEAVE_ERR_MSG))
            break;
        rc = TCL_OK;
    } while (0);
    if (TCL_OK != rc) {
        if (ans) {
            Tcl_DecrRefCount(ans);
        }
    }
    return rc;
}

int CL_Init(Tcl_Interp *ip) {

    OBJ_CL_PROFILING_COMMAND_QUEUED = Tcl_NewStringObj("CL_PROFILING_COMMAND_QUEUED",-1);
    OBJ_CL_PROFILING_COMMAND_SUBMIT = Tcl_NewStringObj("CL_PROFILING_COMMAND_SUBMIT",-1);
    OBJ_CL_PROFILING_COMMAND_START = Tcl_NewStringObj("CL_PROFILING_COMMAND_START",-1);
    OBJ_CL_PROFILING_COMMAND_END = Tcl_NewStringObj("CL_PROFILING_COMMAND_END",-1);
    OBJ_CL_PROFILING_COMMAND_INFO = Tcl_NewStringObj("info",-1);

    Tcl_IncrRefCount(OBJ_CL_PROFILING_COMMAND_QUEUED);
    Tcl_IncrRefCount(OBJ_CL_PROFILING_COMMAND_SUBMIT);
    Tcl_IncrRefCount(OBJ_CL_PROFILING_COMMAND_START);
    Tcl_IncrRefCount(OBJ_CL_PROFILING_COMMAND_END);

    Tcl_CreateObjCommand(ip, POLYNSP "cl::platform", stclPlatformCmd,
                         (ClientData)0, NULL);

    Tcl_CreateObjCommand(ip, POLYNSP "cl::buffer::create", stclBufferCreateCmd,
                         (ClientData)0, NULL);

    Tcl_CreateObjCommand(ip, POLYNSP "cl::buffer::value", stclBufferValueCmd,
                         (ClientData)0, NULL);

    Tcl_CreateObjCommand(ip, POLYNSP "cl::event::move", stclEventMoveCmd,
                         (ClientData)0, NULL);

    Tcl_CreateObjCommand(ip, POLYNSP "cl::event::wait", stclEventWaitCmd,
                         (ClientData)0, NULL);

    Tcl_CreateObjCommand(ip, POLYNSP "cl::event::history", stclEventLogCmd,
                         (ClientData)0, NULL);

    Tcl_LinkVar(ip, POLYNSP "cl::profiling", (char *) &ProfilingEnabled, TCL_LINK_INT);

    const char *intType = "undefined";
    char intSize[3];
    sprintf(intSize, "%d", (int)(sizeof(int)));
    switch (sizeof(int)) {
    case 1:
        intType = "char";
        break;
    case 2:
        intType = "short";
        break;
    case 4:
        intType = "int";
        break;
    case 8:
        intType = "long";
        break;
    }
    Tcl_SetVar(ip, POLYNSP "cl::intType", intType, 0);
    Tcl_SetVar(ip, POLYNSP "cl::intSize", intSize, 0);

    return 0;
}
