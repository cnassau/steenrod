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

NOWEBFILES = 
SRCS = adlin.c linalg.c poly.c prime.c tlin.c tprime.c tptr.c tptrtest.c \
       tpoly.c profile.c tprofile.c

OBJS = $(SRCS:.c=.o)

LIBS = libtlin.so libtprime.so libtptrtest.so libtpoly.so

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

%.tex : %.nw
	$(NOWEAVE) -n  $*.nw  > $*.tex

depend: 
	makedepend -Y -- $(CFLAGS) -- $(SRCS) 2> /dev/null

libtptrtest.so : tptr.o tptrtest.o
	$(LINKSTUB) -o libtptrtest.so tptr.o tptrtest.o

libtprime.so: tprime.o tptr.o prime.o
	$(LINKSTUB) -o libtprime.so tptr.o prime.o tprime.o

libtlin.so:  tlin.o adlin.o prime.o linalg.o tptr.o tprime.o
	$(LINKSTUB) -o libtlin.so tlin.o adlin.o prime.o linalg.o tptr.o tprime.o 

libtpoly.so: tlin.o adlin.o prime.o linalg.o tptr.o tprime.o tpoly.o poly.o
	$(LINKSTUB) -o libtpoly.so tlin.o adlin.o prime.o linalg.o tptr.o tprime.o tpoly.o poly.o

clean:
	$(RM) $(OBJS) $(LIBS) *~  2> /dev/null

reformat:
	refmt.sh *.c *.h 

# DO NOT DELETE -- make depend will overwrite the entries below

adlin.o: adlin.h linalg.h prime.h
linalg.o: linalg.h prime.h
poly.o: poly.h prime.h
prime.o: prime.h
tlin.o: tlin.h tptr.h tprime.h adlin.h linalg.h prime.h
tprime.o: tprime.h tptr.h prime.h
tptr.o: tptr.h
tptrtest.o: tptr.h
tpoly.o: tptr.h tpoly.h tprime.h poly.h prime.h
profile.o: ./profile.h prime.h
tprofile.o: tptr.h tprofile.h ./profile.h prime.h
