#!/usr/bin/tclsh
#
# Script to test the enumeration of Milnor basis elements
#
# Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

load libtenum.so

epol::enumerator x

x conf -genlist {8 }
catch {x basis}

x conf -genlist {8 9}

set gl {}
for {set i 0} {$i<5} {incr i} { lappend gl [list $i $i] }

x conf -prime 2 -ideg 16 -genlist $gl

foreach edg {0 1 2 3 4 5} {
    puts "basis for edeg=$edg"
    x conf -edeg $edg
    puts [x basis]
}

