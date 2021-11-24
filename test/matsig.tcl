# -*- tcl -*-
#
# This file provides a test suite for the signature decomposition
# and the computation of the signature preserving parts of differentials
#
# Copyright (C) 2021 Christian Nassau <nassau@nullhomotopie.de>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

if {[lsearch [namespace children] ::tcltest] == -1} {
    package require tcltest
    namespace import -force ::tcltest::*
}

if {[file exists pkgIndex.tcl]} {
    set dir [pwd]
    source pkgIndex.tcl
    set auto_path something_useless
}
package require Steenrod

namespace import -force steenrod::*

# --------------------------------------------------------------------------

source [file join [file dir [info script]] md5.tcl]

proc setup-test {prime algebra gens1 gens2 diff} {
    enumerator src -prime $prime -algebra $algebra -genlist $gens1
    enumerator dst -prime $prime -algebra $algebra -genlist $gens2
    monomap mmp
    foreach {src dst} $diff {
        mmp add [list 1 0 {} $src] $dst
    }
}

proc test-matrix {ideg edeg -> mvar} {
    src configure -ideg $ideg -edeg $edeg
    dst configure -ideg $ideg -edeg $edeg
    upvar 1 $mvar mat
    #puts [src dimension]x[dst dimension]
    set mat [steenrod::ComputeMatrix src mmp dst]
    set ans [matrix dimensions $mat]
    lappend ans [md5::md5 $mat]
    return $ans
}

proc seqnomap {enm1 enm2} {
    set ans {}
    poly foreach [$enm2 basis] m {
        lappend ans [$enm1 seqno $m]
    }
    return $ans
}

proc test-profile {mvar prof} {
    upvar 1 $mvar mat
    eval enumerator src2 {*}[src configure]
    eval enumerator dst2 {*}[dst configure]
    src2 configure -profile $prof
    dst2 configure -profile $prof
    src2 sigreset
    dst2 sigreset
    set log {}
    while on {
        dst2 configure -signature [src2 cget -signature]
        set a [src2 dimension]
        set b [dst2 dimension]
        if {$a!=0 && $b!=0} {
            #puts ${a}x${b}
            set submatrix [steenrod::ComputeMatrix src2 mmp dst2]
            lappend log $a $b [set subhash [md5::md5 $submatrix]]
            set sm1 [seqnomap src src2]
            set sm2 [seqnomap dst dst2]
            set xmat [matrix extract cols [matrix extract rows $mat $sm1] $sm2]
            set xhash [md5::md5 $xmat]
            if {$subhash ne $xhash} {
                puts "have $subhash:\n$submatrix"
                puts "want $xhash:\n$xmat"
                error "direct submatrix computation failed"
            }
        }
        if {![src2 signext]} break
    }
    return $log
}

tcltest::customMatch list compare-lists
proc compare-lists {a b} {
    if {[llength $a] != [llength $b]} {
        return 0
    }
    foreach x $a y $b {
        if {$x ne $y} {
            return 0
        }
    }
    return yes
}
