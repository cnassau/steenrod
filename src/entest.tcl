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

lappend auto_path .
package require poly

load libtprofile.so

# random integer
proc rint {max} { return [expr int(rand()*$max)] }

proc relem {lst} { return [lindex $lst [rint [llength $lst]]] }

proc bitprint {inp} {
    set inp [expr $inp & 255]
    for {set blist {}} {$inp} {set inp [expr ($inp >> 1)]} {
        lappend blist [expr $inp & 1]
    }
    return Q([join $blist ","])
}

while 1 {

    set prime [relem [list 2 3 5 7]]
    #set prime 2

    set pi [primeinfo::create $prime]
    set ppws [primeinfo::primpows $pi]

    set mindim 0
    set maxdim [rint [lindex $ppws 6]]
    if {$maxdim > 30000} { set maxdim 30000 }
    #puts "prime $prime from dim $mindim to $maxdim"

    set algprof [profile::create]
    set proprof [profile::create]

    set algcore [profile::getCore $algprof]
    set procore [profile::getCore $proprof]

    set last {}

    set alst {}
    for {set i 4} {$i>=0} {incr i -1} {
        lappend alst [lindex $ppws [expr 0 + [rint 6]]]
    }


    set plst {}
    for {set i 4} {$i>=0} {incr i -1} {
        lappend plst [lindex $ppws [rint 3]]
    }

    for {set i 0} {$i<[llength $plst]} {incr i} {
        if {[lindex $plst $i]>[lindex $alst $i]} {
            set plst [lreplace $plst $i $i [lindex $alst $i]]
        }
    }

#        set alst {16 2 2 8 4}
#        set plst {}
    procore::set $algcore 0 1 $alst
    procore::set $procore  0 1 $plst

    puts [format "Algebra profile = %s / %s" \
              [bitprint [procore::getExt $algcore]] [procore::getRed $algcore]]

    puts [format "Subalg. profile = %s / %s" \
              [bitprint [procore::getExt $procore]] [procore::getRed $procore]]

    set dim [expr $mindim + [rint [expr $maxdim - $mindim]]]

    set evn [enumenv::create $pi $algprof $proprof]

    set exm [extmono::create]

    set exmcore [extmono::getCore $exm]

    proc xputs args { 
#         uplevel 1 puts \"$args\" 
    }

    proc randomizeSignature {exm prof} {
        set ecore [extmono::getCore $exm]
        set pe [::procore::getExt $prof]
        set pr [::procore::getRed $prof]
        set ee [::procore::getExt $ecore]
        set er [::procore::getRed $ecore]
        if {[expr $pe & $ee]} { error "pe=$pe, ee=$ee" }
        foreach a $pr b $er {
            set r [rint [expr $a]]
            set r 0
            lappend rlist $r
            lappend x [expr $r + $b]
        }
        #puts "perturbation = $rlist"
        set x [lreplace $x end-1 end]
        ::procore::set $ecore [expr $ee | ([rint 255] & $pe)] 0 $x
    }

    # for {set dim $mindim} {$dim<$maxdim} {incr dim}     
    while {[rint 100]<99} {
        set dim [expr $mindim + [rint [expr $maxdim-$mindim]]]
        set dim [expr $dim / (2*($prime-1))]
        set dim [expr $dim * (2*($prime-1))]

        set sqn [sqninfo::create $evn [expr 1 * $dim]]

        set cnt 0
        if {[extmono::first $exm $evn $dim]} {
            set goon 1
            while {$goon} {
                randomizeSignature $exm $procore 

                set actdeg [expr 2 * ($prime-1) * [procore::getReddeg $exmcore $pi]]
#puts "$actdeg / $dim" 
                set seqnum [sqninfo::getSeqno $sqn $exm $actdeg]
                if {$cnt!=$seqnum} {set cmd puts} else {set cmd xputs}
                eval [list $cmd [format "%5d (=%5d) : %10s : %s" $cnt $seqnum \
                                     [bitprint [procore::getExt $exmcore]] \
                                     [procore::getRed $exmcore]]]
                if {$cnt!=$seqnum} exit
                incr cnt
                set goon [extmono::next $exm $evn] 
            }
        }

#        puts [format "prime %2d dim = %6d : cnt = %6d" $prime $dim $cnt]
        set sqdim [sqninfo::getDim $sqn $dim]
        sqninfo::destroy $sqn
        if {$sqdim!=$cnt} {
            puts "According to seqno count in dimension $dim should be $sqdim, not $cnt"
            exit 1
        }     
        puts [format "dimension %10d count = %5d" $dim $cnt]
    }
    
}
