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
for {set i 0} {$i<1} {incr i} { lappend gl [list $i $i] }

x conf -prime 2 -ideg 16 -genlist $gl
if 0 {
    foreach edg {0 1 2 3 4 5} {
        puts "basis for edeg=$edg"
        x conf -edeg $edg
        x conf -profile {0 0 0 0} -signature  {0 0 0 0}
        puts "profile 0 basis = [x basis]"
        x conf -profile {0 0 1 0}
        puts "profile 1 basis = [x basis]"
        x conf -signature {0 0 1 0}
        puts "profile 1 sig 1 basis = [x basis]"
    }
}

x configure -profile {0 0 0 0} -prime 3 -genlist 0 -ideg 368 -edeg 0
set basis [x basis]
#puts "basis = $basis"

# random integer
proc rint {max} { return [expr int(rand()*$max)] }

proc relem {lst} { return [lindex $lst [rint [llength $lst]]] }


while 1 {
    set p [relem {2 3 5 7 11}]
    #set p 5
    set dim [rint [lindex [prime::primpows $p] 5]]
    set dim [expr int($dim / [prime::tpmo $p]) * [prime::tpmo $p]]

    if 0 {
        x conf -prime 5 -ideg 10
        set aux [x basis]
        x conf -prime 3 -ideg 8
        set aux [x basis]
    }

    #puts "prime $p, degree $dim"
    x configure -prime $p -ideg $dim -genlist 0
    #x configure -genlist {{0 0} {1 50} {2 -50}}
    #x configure -profile {0 0 {1 0 5} 0} -edeg 0
    set basis [x basis]
    
    set cnt 0
    foreach ex $basis { 
        #puts "seqno($ex) = "
        set sqn [x seqno $ex]
        #puts "$sqn"
        if {$cnt!=$sqn} { 
            puts "prime $p, degree $dim"
            puts "seqno($ex) = $sqn, should be $cnt"
            error "inconsistency found!!" 
        }
        incr cnt
    }
    puts [format "basis with %8d elements checked (prime %2d, deg %d)" $cnt $p $dim]
    set bidi [x dim]
    if {$bidi!=$cnt} { error "count mismatch ($bidi!=$cnt)!!" }
}

if 1 {
    # test signature enumeration 

    x conf -prime 2 -profile {0 6 {2 0 1} 0} -ideg 50


    puts "profile = [x cget -pro]"
    x sigre ; puts "signature [x cget -sig]"

    set cnt 0
    while {[x sign]} {
        #puts [incr cnt]
        puts "signature [x cget -sig]" 
    }
    puts [x siglist]
}

