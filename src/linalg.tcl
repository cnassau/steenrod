# Tcl linear algebra package
#
# Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

package require Itcl
catch { namespace import itcl::* }

load libtlin.so

namespace eval linalg {
    proc printVector {vec {maxcol 75}} {
	set dim [::linalg::getVectorDim $vec]
	set max $dim
	if {$max>$maxcol} { set max $maxcol }
	for {set i 0} {$i < $max} {incr i} {
	    puts -nonewline [::linalg::getVecEntry $vec $i]
	}
	if {$max!=$dim} { puts "..." } else { puts "" }
    }


    proc printMatrix {mat {maxrows 20} {maxcols 75}} {
	set dims [::linalg::getMatrixDim $mat]
	set mr [set rows [lindex $dims 0]]
	set mc [set cols [lindex $dims 1]]
	if {$mr>$maxrows} { set mr $maxrows }
	if {$mc>$maxcols} { set mc $maxcols }
	for {set j 0} {$j < $mr} {incr j} {
	    for {set i 0} {$i < $mc} {incr i} {
		puts -nonewline [::linalg::getMatEntry $mat $j $i]	
	    }
	    if {$mc!=$cols} { puts "..." } else { puts "" }
	}
	if {$mr!=$rows} { puts "..." }
    }

}













package provide linalg 1.0




