#!/usr/bin/tclsh
#
# Resolve an algebra and display the results in a canvas
#
# Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

lappend auto_path ../lib

package require Steenrod

array set options {
    -prime   2
    -algebra {}
    -usegui  0
    -maxdim  40
    -maxs    50
}

foreach {opt val} $argv {
    if {![info exist options($opt)]} {
        puts [join [list "unknown option $opt" \
                        "available options: [array names options]"] ", "]
        puts "Example: for A(2) use -algebra \"0 0 {3 2 1} 0\""
        exit 1
    }
    set options($opt) $val
}

# see if options make sense
poly::enumerator x 
if {[catch {
    x co -prime $options(-prime) -algebra $options(-algebra)
    x dim
} err]} {
    puts "cannot set -prime $options(-prime) -algebra $options(-algebra): $err"
    exit 1
}
rename x ""

set p   $options(-prime)
set alg $options(-algebra)

if {![scan $options(-maxdim) %d maxdim]} { 
    puts "expected integer, found $options(-maxdim)" 
    exit 1
}

if {![scan $options(-maxs) %d maxs]} { 
    puts "expected integer, found $options(-maxs)" 
    exit 1
}

if {![scan $options(-usegui) %d usegui]} { 
    puts "expected integer, found $options(-usegui)" 
    exit 1
}

# create empty modules and maps

incr maxs
for {set i -2} {$i<=$maxs} {incr i} {
    poly::enumerator C$i -prime $p -algebra $alg
    poly::monomap d$i
}

# fake generator from Ext^0
C0 configure -genlist 0 

proc matprint {name mat} {
    if 0 {
        set ind "\n    "
        puts -nonewline "$name ([linalg::getdims $mat])=$ind"
        puts [join $mat $ind]
    }
    if 0 {
        puts "$name: [::linalg::getdims $mat]"
    }
}

set maxe $maxs

# newgen is a callback function that can be overwritten in the GUI.
# The GUI will also put a trace on the tridegree variable. When
# we're done we'll call the finito proc.

set tridegree {} 

proc finito {} { puts "Done." }

proc newgen {id s e i diff} {
    puts [format "%3d / %3d : gen %d (diff = %s)" $s $i $id $diff]
}

if {$usegui} {
    source [file join [file dirname [info script]] simplegui.tcl]
}

# Main loop starts here:

newgen 0 -1 0 0 {} ;# fake gen of deg zero

for {set edeg 0} {$edeg<=$maxe} {incr edeg} {

    set maxi [expr $maxdim]

    for {set ideg 0} {$ideg<=$maxi} {incr ideg} {
        
        # We try to deal with each matrix Cs -> Cs-1 just once, which
        # means that we carry the kernel from the s'th loop over into 
        # loop (s+1). However, since we want to add new generators to 
        # Cs+1 at the same time, the old matrix wouldn't exactly fit
        # the changed module, which would result in an error in the 
        # "enumerator decode" routine. For that reason we just record
        # the new generators and do the actual update after we're through
        # with the entire loop.  
        
        set ngenlist {}
        
        for {set sdeg -1} {$sdeg<$maxs} {incr sdeg} {

            # resolve tridegree (s,e,t) == ($sdeg, $edeg, $ideg)

            set tridegree [list $ideg $edeg $sdeg]

            # determine previous, current, and next value of s
            set sp [expr $sdeg-1]
            set sc $sdeg
            set sn [expr $sdeg+1]

            foreach hdg [list $sp $sc $sn] {
                C$hdg configure -edeg $edeg -ideg $ideg 
            }

            # puts "working on ($sdeg, $edeg, $ideg)"

            # compute new matrix:
            if {$sc==-1} {
                if {($edeg==0) && ($ideg==0)} {
                    set nmat 1
                } else {
                    # here C-1 = F_p = 0. create a zero matrix with the
                    # right number of rows.
                    set nmat {}
                    for {set i [C0 dim]} {$i>0} {incr i -1} { lappend nmat 0 }
                }
            } else {
                if 0 {   
                    puts "Matrix:\nC$sn: [C$sn conf]\nC$sc: [C$sc conf]"
                    puts "basis C$sn = [C$sn basis]"
                    puts "basis C$sc = [C$sc basis]"
                    puts "map d$sn = [d$sn list]"
                }

                # using eval here gives nicer error reports
                eval set nmat \[poly::ComputeMatrix C$sn d$sn C$sc\]  
           }

            matprint matrix $nmat
            
            # orthogonalize
            linalg::ortho $p nmat nker

            # now nmat (resp. nker) contains basis of image (resp. ker) 

            matprint kernel $nker
            matprint image  $nmat

            if {$sc>=0} {

                #puts "about to compute oker mod nmat"
                matprint oker $oker
                matprint nmat $nmat

                # assume we have oker from the previous round
                linalg::quot $p oker nmat

                matprint post-oker $oker
                matprint post-nmat $nmat
      
                matprint quotient $oker

                # now oker is the quotient matrix

                set qdims [linalg::getdims $oker]
                set qrows [lindex $qdims 0]

                if {$qrows} { 
                    #puts "($sdeg, $edeg, $ideg) += $qrows gen(s)" 
                    set id [llength [C$sn cget -genlist]]
                }

                for {set i 0} {$i<$qrows} {incr i} {
                    set vct [lindex $oker $i] ;# TODO: create optimized version
                    set diff [C$sc decode $vct]
                    #puts "scheduled gen $id with diff $diff"
                    lappend ngenlist [list $id $sn $edeg $ideg $diff]
                    incr id
                }

            } 

            # rename kernel for use in next round 
            set oker $nker

        }

        # now add new generators 

        foreach desc $ngenlist {
            foreach {id s e i df} $desc break
            set gl [C$s cget -genlist]
            lappend gl [list $id $i $e 0]
            eval C$s configure -genlist \$gl
            eval d$s set \[list 0 0 0 $id\] \$df
            newgen $id [expr $s-1] $e $i $df
        }
    }
}

finito
