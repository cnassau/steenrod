# -*- tcl -*-
#
# This file produces test data templates for the enumerator regression tests
#
# Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

package require Steenrod 1.0

set DATA {}

poly::enumerator ENU

foreach alg {
    {} {0 15 0 0} {0 3 1 0} {0 7 {2 1} 0} {0 1 {0 2 2} 0}
} {

    foreach edeg {-5 -4 -2 -1 0 1 2 3 4 6} {

        foreach p {2 3 5 7 11 13 17} {
            set anm [string map {" " x} [join $alg -]]

            set DATA "set enumtestdata($p-$edeg-$anm-dims) \{\n"
            
            append DATA "\{-prime $p\} \{-genlist 0\} \{-edeg $edeg\} \{-algebra \{$alg\}\}\n"
            
            ::ENU configure -prime $p -genlist 0 -edeg $edeg -type p -algebra $alg

            append DATA "\{ dimtest \{"
            
            set max [lindex [prime::reddegs $p] 4]
            if {$max>50000} {set max 50000}
            if {$max<200} {set max 200}

            set cnt 0
            set type 1 
            set perc 90
            for {set i $max} {$i>-$max} {incr i -1} {
                if {$type && ($i<-20)} { 
                    append DATA "\} \}\n \{ -type n \} \{ dimtest \{"
                    set type 0
                    ::ENU conf -type n
                } 
                ::ENU conf -ideg $i 
                set dm [::ENU dim]
                if {($dm && ([expr int(rand()*100)]<$perc)) || ([expr int(rand()*1000)]<3)} {
                    set perc [expr 4 + 0.95*$perc]
                    append DATA " $i "
                    if {$dm} { append DATA x } else { append DATA n }
                    incr cnt
                    if {0 == [expr $cnt % 7]}  {
                        append DATA "\} \}\n \{ seqtest $i \} \{ dimtest \{"
                    }
                }
            }

            append DATA "\} \}\n"
            
            append DATA "\}\n\n\n"
            puts $DATA

        }

    }
}
