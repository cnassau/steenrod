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
CFLAGS = -O2 -g -Wall -I. -DUSE_TCL_STUBS

TCLDIR = /home/cn/selfmade-tcl/include/

NOTANGLE = notangle
NOWEAVE  = noweave

%.c : %.nw
	$(NOTANGLE) -L -R$*.c $(NOWEBFILES) > $*.c

%.h : %.nw
	$(NOTANGLE) -L -R$*.h $(NOWEBFILES) > $*.h

%.o : %.c
	$(CC) $(CFLAGS) -I $(TCLDIR) -c $*.c

%.ps : %.dvi
	dvips $*.dvi

%.tex : %.nw
	$(NOWEAVE) -n  $*.nw  > $*.tex

.PHONY: all

all: libtptrtest.so libtprime.so linalg.o adlin.o

tptr.o: tptr.h tptr.c
	$(CC) $(CFLAGS) -I $(TCLDIR) -c tptr.c

tptrtest.o : tptr.h tptrtest.c
	$(CC) $(CFLAGS) -I $(TCLDIR) -c tptrtest.c

libtptrtest.so : tptr.o tptrtest.o
	cc -shared -o libtptrtest.so tptr.o tptrtest.o

linalg.o: linalg.c linalg.h prime.h
adlin.o : adlin.c adlin.h linalg.h prime.h
prime.o: prime.c prime.h

tprime.o: tprime.c prime.h

libtprime.so: tprime.o tptr.o prime.o
	cc -shared -o libtprime.so tptr.o prime.o tprime.o
