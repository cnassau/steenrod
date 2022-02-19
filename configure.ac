#!/bin/bash -norc
dnl	This file is an input file used by the GNU "autoconf" program to
dnl	generate the file "configure", which is run during Tcl installation
dnl	to configure the system for the local environment.
#
# RCS: @(#) $Id$

#-----------------------------------------------------------------------
# Sample configure.in for Tcl Extensions.  The only places you should
# need to modify this file are marked by the string __CHANGE__
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# __CHANGE__
# Set your package name and version numbers here.
#
# This initializes the environment with PACKAGE_NAME and PACKAGE_VERSION
# set as provided.  These will also be added as -D defs in your Makefile
# so you can encode the package version directly into the source files.
#-----------------------------------------------------------------------

AC_INIT([Steenrod], [2.2])

#--------------------------------------------------------------------
# Call TEA_INIT as the first TEA_ macro to set up initial vars.
# This will define a ${TEA_PLATFORM} variable == "unix" or "windows"
# as well as PKG_LIB_FILE and PKG_STUB_LIB_FILE.
#--------------------------------------------------------------------

TEA_INIT([3.7])

AC_CONFIG_AUX_DIR(tclconfig)

#--------------------------------------------------------------------
# Load the tclConfig.sh file
#--------------------------------------------------------------------

TEA_PATH_TCLCONFIG
TEA_LOAD_TCLCONFIG

#--------------------------------------------------------------------
# Load the tkConfig.sh file if necessary (Tk extension)
#--------------------------------------------------------------------

#TEA_PATH_TKCONFIG
#TEA_LOAD_TKCONFIG

#-----------------------------------------------------------------------
# Handle the --prefix=... option by defaulting to what Tcl gave.
# Must be called after TEA_LOAD_TCLCONFIG and before TEA_SETUP_COMPILER.
#-----------------------------------------------------------------------

TEA_PREFIX

#-----------------------------------------------------------------------
# Standard compiler checks.
# This sets up CC by using the CC env var, or looks for gcc otherwise.
# This also calls AC_PROG_CC, AC_PROG_INSTALL and a few others to create
# the basic setup necessary to compile executables.
#-----------------------------------------------------------------------

TEA_SETUP_COMPILER

AC_PROG_CXXCPP
AC_PROG_CXX

# openCL support

AC_MSG_CHECKING([whether to use OpenCL instructions])

    AC_ARG_ENABLE(opencl,
        AC_HELP_STRING([--enable-opencl],
            [use OpenCL support (default: no)]),
        [opencl=$enableval], [opencl=no])

if test "$opencl" != "no"; then
 LIBSSAVED="$LIBS"
 LIBS="$LIBS -lOpenCL"
 AC_LINK_IFELSE([AC_LANG_PROGRAM(
     [[#define CL_TARGET_OPENCL_VERSION 120
       #include <CL/opencl.h>
       void f(void) {
         cl_platform_id platform_id = NULL;
         cl_uint ret_num_platforms;
         clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
       }]], [f();])],
           [useopencl=yes],
           [useopencl=no])
else
    useopencl=no
fi

if test "$useopencl" = "no"; then
    if test "$opencl" = "yes"; then
        AC_MSG_ERROR( "OpenCL requested but not available", 1)
    fi
    LIBS="$LIBSSAVED"
else
    LDFLAGS="$LDFLAGS -lOpenCL"
fi

AC_MSG_RESULT([$useopencl])

AS_IF([test "$useopencl" = yes], [AC_DEFINE(USEOPENCL,1)])
AS_IF([test "$useopencl" = no], [AC_DEFINE(USEOPENCL,0)])

# detect SSE2-capable processor

AC_MSG_CHECKING([whether to use sse2 instructions])

    AC_ARG_ENABLE(sse2,
	AC_HELP_STRING([--enable-sse2],
	    [use sse2 instructions (default: detect)]),
	[sse2=$enableval], [sse2=detect])

if test "$sse2" != "no"; then

    AC_TRY_LINK([
	    #include <emmintrin.h>
	    void f(void) {__m128i var = _mm_set_epi16(7,6,5,4,3,2,1,0);}], [f();],
	usesse2=yes,
	usesse2=no)
else
    usesse2=no
fi

if test "$usesse2" = "no"; then
    if test "$sse2" = "yes"; then
	AC_MSG_ERROR( "sse2 requested but not available", 1)
    fi
fi

AC_MSG_RESULT([$usesse2])

AS_IF([test "$usesse2" = yes], [AC_DEFINE(USESSE2,1)])

#-----------------------------------------------------------------------
# __CHANGE__
# Specify the C source files to compile in TEA_ADD_SOURCES,
# public headers that need to be installed in TEA_ADD_HEADERS,
# stub library C source files to compile in TEA_ADD_STUB_SOURCES,
# and runtime Tcl library files in TEA_ADD_TCL_SOURCES.
# This defines PKG(_STUB)_SOURCES, PKG(_STUB)_OBJECTS, PKG_HEADERS
# and PKG_TCL_SOURCES.
#-----------------------------------------------------------------------

srcdir=../src
TEA_ADD_SOURCES([
	 enum.cc	   mult.cc      steenrod.cc	 tprime.cc
	 adlin.cc	  hmap.cc	linalg.cc   poly.cc   tptr.cc
		  linwrp.cc   prime.cc     tenum.cc
	 linwrp2.cc   tlin.cc  common.cc	 	momap.cc  tpoly.cc
])
TEA_ADD_HEADERS()
#[adlin.h   hmap.h    linwrp.h  poly.h	scrobjy.h   steenrod.h	tpoly.h
#common.h  lepar.h   momap.h   prime.h	secmult2.h  tenum.h	tprime.h
#enum.h	  linalg.h  mult.h    refmt.sh	ssedefs.h   tlin.h	tptr.h])
TEA_ADD_INCLUDES([-I $srcdir])
TEA_ADD_LIBS([])
TEA_ADD_CFLAGS([])
TEA_ADD_STUB_SOURCES([])
TEA_ADD_TCL_SOURCES([startup.tcl algebra.tc conj.tc a2nd.tc opencl.tcl])
AS_IF([test "$useopencl" = yes], [TEA_ADD_SOURCES([opencl.cc])])

#--------------------------------------------------------------------
# __CHANGE__
# A few miscellaneous platform-specific items:
#
# Define a special symbol for Windows (BUILD_sample in this case) so
# that we create the export library with the dll.
#
# Windows creates a few extra files that need to be cleaned up.
# You can add more files to clean if your extension creates any extra
# files.
#
# TEA_ADD_* any platform specific compiler/build info here.
#--------------------------------------------------------------------

# Add pkgIndex.tcl if it is generated in the Makefile instead of ./configure
# and change Makefile.in to move it from CONFIG_CLEAN_FILES to BINARIES var.
#CLEANFILES="pkgIndex.tcl"
if test "${TEA_PLATFORM}" = "windows" ; then
    AC_DEFINE(BUILD_sample, 1, [Build windows export dll])
    CLEANFILES="$CLEANFILES *.lib *.dll *.exp *.ilk *.pdb vc*.pch"
    #TEA_ADD_SOURCES([win/winFile.c])
    #TEA_ADD_INCLUDES([-I\"$(${CYGPATH} ${srcdir}/win)\"])
else
    # Ensure no empty else clauses
    :
    #TEA_ADD_SOURCES([unix/unixFile.c])
    #TEA_ADD_LIBS([-lsuperfly])
fi
AC_SUBST(CLEANFILES)

#--------------------------------------------------------------------
# __CHANGE__
# Choose which headers you need.  Extension authors should try very
# hard to only rely on the Tcl public header files.  Internal headers
# contain private data structures and are subject to change without
# notice.
# This MUST be called after TEA_LOAD_TCLCONFIG / TEA_LOAD_TKCONFIG
#--------------------------------------------------------------------

TEA_PUBLIC_TCL_HEADERS
#TEA_PRIVATE_TCL_HEADERS

#TEA_PUBLIC_TK_HEADERS
#TEA_PRIVATE_TK_HEADERS
#TEA_PATH_X

#--------------------------------------------------------------------
# Check whether --enable-threads or --disable-threads was given.
# This auto-enables if Tcl was compiled threaded.
#--------------------------------------------------------------------

TEA_ENABLE_THREADS

#--------------------------------------------------------------------
# The statement below defines a collection of symbols related to
# building as a shared library instead of a static library.
#--------------------------------------------------------------------

TEA_ENABLE_SHARED

#--------------------------------------------------------------------
# This macro figures out what flags to use with the compiler/linker
# when building shared/static debug/optimized objects.  This information
# can be taken from the tclConfig.sh file, but this figures it all out.
#--------------------------------------------------------------------

TEA_CONFIG_CFLAGS

#--------------------------------------------------------------------
# Set the default compiler switches based on the --enable-symbols option.
#--------------------------------------------------------------------

TEA_ENABLE_SYMBOLS

#--------------------------------------------------------------------
# Everyone should be linking against the Tcl stub library.  If you
# can't for some reason, remove this definition.  If you aren't using
# stubs, you also need to modify the SHLIB_LD_LIBS setting below to
# link against the non-stubbed Tcl library.  Add Tk too if necessary.
#--------------------------------------------------------------------

AC_DEFINE(USE_TCL_STUBS, 1, [Use Tcl stubs])
#AC_DEFINE(USE_TK_STUBS, 1, [Use Tk stubs])

#--------------------------------------------------------------------
# This macro generates a line to use when building a library.  It
# depends on values set by the TEA_ENABLE_SHARED, TEA_ENABLE_SYMBOLS,
# and TEA_LOAD_TCLCONFIG macros above.
#--------------------------------------------------------------------

TEA_MAKE_LIB

#--------------------------------------------------------------------
# Determine the name of the tclsh and/or wish executables in the
# Tcl and Tk build directories or the location they were installed
# into. These paths are used to support running test cases only,
# the Makefile should not be making use of these paths to generate
# a pkgIndex.tcl file or anything else at extension build time.
#--------------------------------------------------------------------

TEA_PROG_TCLSH
#TEA_PROG_WISH

#--------------------------------------------------------------------
# Finally, substitute all of the various values into the Makefile.
# You may alternatively have a special pkgIndex.tcl.in or other files
# which require substituting th AC variables in.  Include these here.
#--------------------------------------------------------------------

AC_OUTPUT([Makefile $srcdir/../pkgIndex.tcl])
