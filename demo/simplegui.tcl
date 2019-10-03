#!/usr/bin/tclsh
#
# A (very) simple user interface for the demo resolver
#
# Copyright (C) 2004 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

package require Tk

# Create frame for title, degree info, and buttons:

set tpf [frame .tpf]

pack [button $tpf.cls -text "Close" -command exit] -side right \
    -fill none -expand 0

set algname $options(-algebra)
if {$algname==""} { set algname "full algebra" }

pack [label $tpf.adv \
          -text "Settings: p = $options(-prime), A = \{$algname\}"] \
    -side left -expand 0 -fill x -anchor w

pack [label $tpf.plab -text "  B ="] -side left -expand 0 -fill x -anchor w
pack [label $tpf.prof -textvariable ::theprofile] \
    -side left -expand 0 -fill x -anchor w

pack [label $tpf.pmh -text "  homology ="] -side left -expand 0 -fill x -anchor w
pack [label $tpf.plmh -textvariable ::mathomdim] \
    -side left -expand 0 -fill x -anchor w

pack [label $tpf.pml -text "  lifting ="] -side left -expand 0 -fill x -anchor w
pack [label $tpf.plml -textvariable ::matliftdim] \
    -side left -expand 0 -fill x -anchor w


# now create frame for canvas and scrollbars:

set cvf [frame .cvf]

set cvs [canvas $cvf.c -bg linen -relief sunken -bd 2]
set scv [scrollbar $cvf.sch -orient vertical -command "$cvs yview"]
set sch [scrollbar .scv -orient horizontal -command "$cvs xview"]
$cvs configure -yscrollcommand "$scv set" -xscrollcommand "$sch set" 

pack $sch -side bottom -expand 0 -fill x
pack $scv -side right -expand 0 -fill y 
pack $cvs -side top -expand 1 -fill both

pack $tpf -side top -expand 0 -fill both 
pack $cvf -side top -expand 1 -fill both

wm geometry . 1240x900

set scale 5

proc tocoord x { return [expr $x*$::scale]m }

set maxX 10
set maxY 100

proc updateScrollRegion {} {
    global cvs maxY maxX
    $cvs configure -scrollregion [list [::tocoord -1.5] [::tocoord -$maxY] \
                                      [::tocoord $maxX] [::tocoord 1.5]]
    $cvs yview moveto 1
}

updateScrollRegion

set dotfile @[file join [file dirname [info script]] dot]

proc addDot {x y edeg id s} { 
    global cvs maxY maxX dotfile

    set colval [expr 1-exp(.15*-$edeg)]
    set colval [expr 80 + int(155 * $colval)]
    set bval 80
    set col [format "\#%0x2d%0x2d%0x2d" $colval $colval $bval]

    while {[info exists ::clst($x,$y)]} {
        set x [expr $x-.11]
        set y [expr $y+.1]
    }
    set ::clst($x,$y) {}

    if {$x>$maxX} { set maxX [expr $x+10] ; updateScrollRegion }
    if {$y>$maxY} { set maxY [expr $y+10] ; updateScrollRegion }

    set ::dots($s,$id) [$cvs create bitmap [::tocoord $x] [::tocoord -$y] \
                            -foreground $col -bitmap $dotfile]
}

# finally, here are our "callbacks":

set wpl {}

proc degchange {name1 name2 op} {
    global wpl cvs
    foreach {s i e} $::tridegree break
    # $::statlab configure -text "Current tridegree (s,t,e) = ($s,$i,$e)"
    if {$::viewtypeeven} {set i [expr {$i/2}]}
    incr s 1 

    if {$wpl != {}} { $cvs delete $wpl } 
    set x [expr $i - $s]
    set y [expr -$s]

    set wpl [$cvs create polygon \
                 [::tocoord [expr $x-.4]] [::tocoord [expr $y-.4]] \
                 [::tocoord [expr $x+.4]] [::tocoord [expr $y-.4]] \
                 [::tocoord [expr $x+.4]] [::tocoord [expr $y+.4]] \
                 [::tocoord [expr $x-.4]] [::tocoord [expr $y+.4]] \
                 -fill red -outline black ]

    update
}

trace add variable tridegree write degchange

array set dpat {
    0 1
    1 1
    2 {4 2}
    3 {2 8}
    4 {1 6}
}

proc addLine {id tp} {
    global cvs currs currgenid dpat

    if {$tp>3} return
    set fr [$cvs coords $::dots($currs,$currgenid)]
    set to [$cvs coords $::dots([expr $currs-1],$id)]
    
    $cvs create line [lindex $fr 0] [lindex $fr 1] \
        [lindex $to 0] [lindex $to 1] -dash $dpat($tp)
}

# see if m signals h_i or v_0 multiples
proc myfilter {m} {
    if {[mono rlength $m] > 1} { return -code continue }
    set ext [lindex $m 1]
    if {$ext==0} {
        set v [lindex $m 2] 
        set i 1
        foreach pow [::prime $::p powers] {
            if {$pow==$v} {
                addLine [lindex $m 3] $i
                return -code continue 
            }
            if {[incr i]>4} break
        }
        return -code continue
    }
    if {$ext==1} {
        if {[lindex $m 2] == {}} { addLine [lindex $m 3] 0 }
        return -code continue
    }
    return -code continue
}
    
proc newgen {id s e i diff} {
    # puts [format "%3d / %3d : gen %d (diff = %s)" $s $i $id $diff]
    set ::currgenid $id
    set ::currs $s
    if {$::viewtypeeven} {set i [expr {$i/2}]}
    addDot [expr {$i-$s}] $s $e $id $s
    poly split $diff ::myfilter

    update
}

# we arrange to call update during computations
set steenrod::_progvarname ::ourProgVar
proc myupdate {args} { update }
trace add variable ::ourProgVar write myupdate

update

rename ::error error.orig
proc error {msg} {
    after idle [list error.orig $msg]
    update
}
