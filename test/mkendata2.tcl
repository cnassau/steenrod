# -*- tcl -*-
#
# Another file that produces test data templates for the enumerator tests
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

foreach {alg pro} {
    {} {0 14 0 0} 
    {} {0 3 1 0} 
    {} {0 7 {2 1} 0} 
    {} {0 1 {0 2 2} 0}
    {0 7 {3 1} 0} {0 3 {7 7 0} 0} 
    {0 15 {3 2 1} 0} {0 3 {1} 0} 
    {0 15 {3 2 1} 0} {0 7 {2 1} 0} 
    {0 31 {0 0 1} 0} {0 15 {0} 0} 
} {


    foreach p {2 3 5} {
        set anm [string map {" " x} [join $alg -]]
        set pnm [string map {" " x} [join $pro -]]

        set DATA "set enumtestdata($p-$anm-$pnm-sig) \{\n"
        
        append DATA "\{-prime $p\} \{-genlist 0\} "
        append DATA "\{-algebra \{$alg\}\} \{-profile \{$pro\}\}\n"
        
        ::ENU configure -prime $p -genlist 0 -type p -profile $pro -algebra $alg
 
        set max [lindex [prime::reddegs $p] 4]
        if {$max>2000} {set max 2000}
        if {$max<20} {set max 20}

        set cnt 0
        set type 1 
        set perc 0.95
        for {set edeg 0} {$edeg<6} {incr edeg} {
            set neededg 1
            set oslt -666
            puts stderr "p=$p edeg=$edeg alg=$alg profile=$pro"
            for {set i $max} {$i>-$max} {incr i -1} {
                ::ENU conf -ideg $i -edeg $edeg
                set slt [::ENU sigl]
                if {($oslt != [llength $slt]) || ([expr rand() < 0.001]) } {
                    set oslt [llength $slt]
                    if {$neededg} {
                        set neededg 0
                        append DATA "\{-edeg $edeg\} "
                    }
                    if {$type && ($i<0)} { 
                        append DATA " \{-type n \} \{-edeg -$edeg\} "
                        set type 0
                        ::ENU conf -type n -edeg -$edeg
                    } 
                    if {(!$type) && ($i>=0)} { 
                        append DATA " \{-type p \}  \{-edeg $edeg\} "
                    set type 1
                        ::ENU conf -type p -edeg $edeg
                    } 
                    append DATA "\{-ideg $i\} \{ sigcheck n\}\n"
                }
            }
        }
        
        append DATA "\}\n\n\n"
        puts $DATA

    }

}
