# Main makefile
#
# Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

CC      = gcc -fPIC
CFLAGS = -O2 -g -Wall -I. -DUSE_TCL_STUBS 

TCLDIR = /home/cn/selfmade-tcl/include/

NOTANGLE = notangle
NOWEAVE  = noweave

NOWEBFILES = mult.nw
SRCS = adlin.c linalg.c linwrp.c tlin.c prime.c tprime.c tptr.c \
       poly.c tpoly.c mult.c enum.c tenum.c momap.c steenrod.c

OBJS = $(SRCS:.c=.o)

LIBS = libsteenrod.so

LINKSTUB = cc -shared -L/home/cn/selfmade-tcl/lib/ -ltclstub8.4

.PHONY: depend all clean distclean objects libraries install

all: objects libraries

objects: $(OBJS)

libraries: $(LIBS)

install: libraries
	instlib.tcl $(LIBS)

%.c : %.nw
	$(NOTANGLE) -L -R$*.c $(NOWEBFILES) > $*.c

%.h : %.nw
	$(NOTANGLE) -L -R$*.h $(NOWEBFILES) > $*.h

%.o : %.c
	$(CC) $(CFLAGS) -I$(TCLDIR) -c $*.c

%.ps : %.dvi
	dvips $*.dvi

.PHONY: FORCE

%.dvi : %.tex FORCE
	latex $*.tex

%.tex : %.nw
	$(NOWEAVE) -delay $*.nw  > $*.tex

depend: 
	makedepend -Y -- $(CFLAGS) -- $(SRCS) 2> /dev/null

libsteenrod.so : $(OBJS)
	$(LINKSTUB) -o libsteenrod.so $(OBJS)

clean:
	$(RM) $(OBJS) $(LIBS) 2> /dev/null

distclean: clean
	$(RM) *~ *.bak 2> /dev/null

reformat:
	refmt.sh *.c *.h 

.PHONY: refmt

refmt: reformat

# DO NOT DELETE -- make depend will overwrite the entries below

adlin.o: adlin.h linalg.h prime.h common.h
linalg.o: linalg.h prime.h common.h
linwrp.o: linwrp.h prime.h common.h linalg.h adlin.h
tlin.o: tlin.h tptr.h common.h tprime.h prime.h linwrp.h
prime.o: prime.h common.h
tprime.o: tprime.h prime.h common.h tptr.h
tptr.o: tptr.h common.h
poly.o: poly.h common.h prime.h mult.h
tpoly.o: tptr.h common.h tpoly.h poly.h prime.h tprime.h mult.h
mult.o: mult.h common.h prime.h poly.h
enum.o: enum.h poly.h common.h prime.h
tenum.o: tprime.h prime.h common.h tpoly.h tptr.h poly.h tlin.h linwrp.h
tenum.o: tenum.h enum.h
momap.o: tprime.h prime.h common.h tpoly.h tptr.h poly.h momap.h
steenrod.o: tprime.h prime.h common.h tpoly.h tptr.h poly.h tlin.h linwrp.h
steenrod.o: steenrod.h momap.h tenum.h enum.h mult.h
