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
SRCS = adlin.c linalg.c poly.c prime.c tlin.c tprime.c tptr.c tptrtest.c \
       tpoly.c profile.c tprofile.c maps.c tmap.c mult.c linwrp.c

OBJS = $(SRCS:.c=.o)

LIBS = libtlin.so libtprime.so libtptrtest.so libtpoly.so libtprofile.so libtmap.so

LINKSTUB = cc -shared -L/home/cn/selfmade-tcl/lib/ -ltclstub8.4

.PHONY: depend all clean objects libraries

all: objects libraries

objects: $(OBJS)

libraries: $(LIBS)

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

libtptrtest.so : tptr.o tptrtest.o
	$(LINKSTUB) -o libtptrtest.so tptr.o tptrtest.o

libtprime.so: tprime.o tptr.o prime.o
	$(LINKSTUB) -o libtprime.so tptr.o prime.o tprime.o

libtlin.so:  tlin.o adlin.o prime.o linalg.o tptr.o tprime.o linwrp.o
	$(LINKSTUB) -o libtlin.so tlin.o adlin.o prime.o linalg.o tptr.o tprime.o linwrp.o

libtpoly.so: prime.o tptr.o tprime.o tpoly.o poly.o mult.o
	$(LINKSTUB) -o libtpoly.so prime.o tptr.o tprime.o tpoly.o poly.o mult.o

libtprofile.so: tprime.o tptr.o prime.o profile.o tprofile.o
	$(LINKSTUB) -o libtprofile.so tprime.o tptr.o prime.o profile.o tprofile.o 

libtmap.so: tmap.o maps.o tptr.o 
	$(LINKSTUB) -o libtmap.so tmap.o maps.o tptr.o 

clean:
	$(RM) $(OBJS) $(LIBS) *~ *.bak 2> /dev/null

reformat:
	refmt.sh *.c *.h 

.PHONY: refmt

refmt: reformat

# DO NOT DELETE -- make depend will overwrite the entries below

adlin.o: adlin.h linalg.h prime.h common.h
linalg.o: linalg.h prime.h common.h
poly.o: poly.h common.h prime.h mult.h
prime.o: prime.h common.h
tlin.o: tlin.h tptr.h common.h tprime.h prime.h adlin.h linalg.h
tprime.o: tprime.h prime.h common.h tptr.h
tptr.o: tptr.h common.h
tptrtest.o: tptr.h common.h
tpoly.o: tptr.h common.h tpoly.h poly.h prime.h tprime.h
profile.o: profile.h prime.h common.h
tprofile.o: tptr.h common.h tprime.h prime.h tprofile.h profile.h
maps.o: maps.h common.h prime.h profile.h
tmap.o: tptr.h common.h tmap.h tprofile.h profile.h prime.h maps.h
mult.o: mult.h common.h prime.h poly.h
