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

# SRCS = adlin.c  linalg.c  prime.c  tlin.c  tprime.c  tptr.c  tptrtest.c
OBJS = adlin.o linalg.o poly.o prime.o tlin.o tprime.o tptr.o tptrtest.o

.PHONY: depend all clean objects

objects: $(OBJS)

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

all: libtptrtest.so libtprime.so libtlin.so

depend: 
	makedepend -Y -- $(CFLAGS) -- $(SRCS) 2> /dev/null

libtptrtest.so : tptr.o tptrtest.o
	cc -shared -o libtptrtest.so tptr.o tptrtest.o

libtprime.so: tprime.o tptr.o prime.o
	cc -shared -o libtprime.so tptr.o prime.o tprime.o

libtlin.so:  tlin.o adlin.o prime.o linalg.o tptr.o tprime.o
	cc -shared -o libtlin.so tlin.o adlin.o prime.o linalg.o tptr.o tprime.o -L/home/cn/selfmade-tcl/lib/ -ltclstub8.4

clean:
	rm $(OBJS)

# DO NOT DELETE -- make depend will overwrite the entries below

adlin.o: adlin.h linalg.h prime.h
linalg.o: linalg.h prime.h
prime.o: prime.h
tlin.o: tlin.h tptr.h tprime.h adlin.h linalg.h prime.h
tprime.o: tprime.h tptr.h prime.h
tptr.o: tptr.h
tptrtest.o: tptr.h
