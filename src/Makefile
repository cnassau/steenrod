# Main makefile
#
# Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

CC      = gcc
CCFLAGS = -O2 -g -Wall -I. -DUSE_TCL_STUBS

TCLDIR = /home/cn/selfmade-tcl/include/

all: libtptrtest.so

tptr.o: tptr.h tptr.c
	$(CC) $(CCFLAGS) -I $(TCLDIR) -c tptr.c

tptrtest.o : tptr.h tptrtest.c
	$(CC) $(CCFLAGS) -I $(TCLDIR) -c tptrtest.c

libtptrtest.so : tptr.o tptrtest.o
	cc -shared -o libtptrtest.so tptr.o tptrtest.o
