# Main makefile
#
# Copyright (C) 2004-2007 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

STEENROD_VERSION=0.7632

SYSTEM = Unix

TCLSH = tclsh

SED = sed

ifeq ($(SYSTEM),Windows)
  CC     = gcc -mdll -mno-cygwin
  CFLAGS = -O2 -g -Wall -I.  -DDLLBUILD

  TCLINCDIR = c:/MinGW/include/
  TCLLIBDIR = c:/MinGW/lib/
  TCLSTUBLIB = tclstub84
  TCLLIB = tcl84
  STUBLINK = gcc -mdll -mno-cygwin -L$(TCLLIBDIR) -lmsvcrt
else
  CC     = gcc -fPIC
  CFLAGS =  -O2 -g -Wall -I. -DUSE_TCL_STUBS

  TCLINCDIR = /usr/include/
  TCLLIBDIR = /usr/lib64 /usr/lib
  TCLSTUBLIB = tclstub8.4
  TCLLIB = tcl8.4
  STUBLINK = gcc -shared $(CFLAGS) $(foreach i,$(TCLLIBDIR),-L$(i))
endif

LIBDIRS = $(foreach i,$(TCLLIBDIR),-L$(i))

CFLAGS += -DSTEENROD_VERSION=\"$(STEENROD_VERSION)\"
CFLAGS += -DUSE_TCL_STUBS -DBUILD_steenrod -DTCL_STORAGE_CLASS=DLLEXPORT 
#CFLAGS += -fprofile-arcs -ftest-coverage -pg
#CFLAGS += -DUSE_VERB_ALLOC=1

# uncomment next line if you have a 64-bit 686 processor with sse2 support
CFLAGS += -msse2 -DUSESSE2

NOTANGLE = notangle
NOWEAVE  = noweave

NOWEBFILES = mult.nw

SRCS = adlin.c linalg.c linwrp.c tlin.c prime.c tprime.c tptr.c \
       poly.c tpoly.c mult.c enum.c tenum.c momap.c steenrod.c common.c \
       hmap.c linwrp2.c lepar.c legram.c algebra.c secmult2.c scrobjy.c

OBJS = $(SRCS:.c=.o)

LIBS = libsteenrod.so

.PHONY: depend all clean distclean objects libraries install

all: objects libraries

objects: $(OBJS)

libraries: $(LIBS)

install: libraries steenrodsh
	$(TCLSH) instlib.tcl $(LIBS)
	(cd ../lib; echo "pkg_mkIndex ." | $(TCLSH))
	mkdirhier ../bin
	cp steenrodsh ../bin

%.c : %.tc
	@echo "const char *tc_$* = " > $*.c
	$(SED) 's/\\/\\\\/g' $*.tc | $(SED) 's/\"/\\\"/g' | $(SED) 's/.*/\"\0\\n\"/g' >> $*.c
	@echo ";" >> $*.c   

%.c : %.nw
	$(NOTANGLE) -L -R$*.c $(NOWEBFILES) > $*.c

%.h : %.nw
	$(NOTANGLE) -L -R$*.h $(NOWEBFILES) > $*.h

%.o : %.c
	$(CC) $(CFLAGS) -I$(TCLINCDIR) -c $*.c

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
	$(STUBLINK) -o libsteenrod.so $(OBJS) -l$(TCLSTUBLIB)

steenrodsh : $(OBJS) steenrodsh.c
	$(CC) -o steenrodsh $(CFLAGS) -I$(TCLINCDIR) $(OBJS) steenrodsh.c $(LIBDIRS) -l$(TCLLIB)

clean:
	$(RM) $(OBJS) $(LIBS) 2> /dev/null

distclean: clean
	$(RM) *~ *.bak 2> /dev/null

reformat:
	refmt.sh *.c *.h 

.PHONY: refmt

refmt: reformat

test: steenrodsh
	./steenrodsh ../test/all.tcl

bigtest: steenrodsh
	$(SH) (TCLTEST_OPTIONS="-constraints BIGTEST"; export TCLTEST_OPTIONS; ./steenrodsh ../test/all.tcl)

lemon: lemon.c 
	$(CC) -o lemon lemon.c

legram.c: legram.y lemon lempar.c
	./lemon legram.y

legram.h: legram.y lemon lempar.c
	./lemon legram.y
	touch legram.h

# DO NOT DELETE -- make depend will overwrite the entries below

adlin.o: adlin.h linalg.h prime.h common.h
linalg.o: linalg.h prime.h common.h
linwrp.o: linwrp.h prime.h common.h linalg.h adlin.h
tlin.o: steenrod.h momap.h poly.h common.h prime.h tenum.h enum.h linwrp.h
tlin.o: tlin.h tptr.h tprime.h
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
steenrod.o: steenrod.h momap.h tenum.h enum.h mult.h hmap.h lepar.h
steenrod.o: secmult2.h scrobjy.h
common.o: common.h
hmap.o: tprime.h prime.h common.h tpoly.h tptr.h poly.h hmap.h
linwrp2.o: linwrp.h prime.h common.h
lepar.o: lepar.h legram.h
legram.o: lepar.h
secmult2.o: secmult2.h tptr.h common.h tprime.h prime.h tpoly.h poly.h
scrobjy.o: scrobjy.h
