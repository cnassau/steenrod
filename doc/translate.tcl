######################################################################
# $Id$
# Created: 9 Aug 2000
# Copyright (C) 2000, Joe English.  All rights reserved.
######################################################################
#
# Description:
#	...
#

package require Tcl 8.2

# Load either TclXML or tDOM.  Either one wil work.
#
if {[catch { package require xml }]} {
    set errmsg $::errorInfo
    if {[catch { package require tdom}]} {
	append errmsg "\n$::errorInfo"
	return -code error -errorinfo $errmsg "Can't load xml or tDOM package"
    }
}

#; %%% package require lpd

namespace eval translate { 
    variable transpec
    variable diversions
    variable stringmaps
    set diversions(default) ""
    set stringmaps(identity) [list A A]
    ;# The reason for the Randian identity map is that using just plain
    ;# [list] seems to trigger a coredump in Tcl 8.4a1...
}

# translate::stringmap name  ? s1 s2 ... ? --
#	Defines, inquires, or modifies a named string map.
#
proc translate::stringmap {mapname {charmap {}}} {
    variable stringmaps
    if {![llength $charmap]} {
    	return $stringmaps($mapname)
    } 
    foreach {src rep} $charmap {
    	lappend stringmaps($mapname) $src $rep
    }
}


######################################################################
#
# Output routines:
#
# Output goes through several buffers on its way to the final destination:
#
# translate::text --
#	appends text to the current diversion, after filtering
#	through the current string map.
# translate::output --
#	appends text to the current diversion without any filtering.
# translate::flush --
#	sends text to the current output procedure, which by default is:
# translate::write --
#	which writes the text to the current output file
#

proc translate::write {text} {
    variable transpec
    puts -nonewline $transpec(outputFile) $text
}

proc translate::writeln {{text ""}} {
    variable transpec
    puts $transpec(outputFile) $text
}

proc translate::flush {} {
    variable transpec
    $transpec(outputProc) [translate::undivert $transpec(diversion)]
}

proc translate::undivert {diversion} {
    variable diversions
    if {[info exists diversions($diversion)]} {
	set result $diversions($diversion)
    } else {
    	set result ""
    }
    set diversions($diversion) ""
    return $result
}

proc translate::output {text} {
    variable transpec
    variable diversions
    append diversions($transpec(diversion)) $text
}

proc translate::text {text} {
    variable transpec
    variable diversions
    variable stringmaps
    append diversions($transpec(diversion)) \
    	[string map $stringmaps($transpec(stringmap)) $text]
}

######################################################################
#
# XML event handlers:
#
proc translate::characterdata {text} {
    variable transpec
    if {$transpec(ignorews) && [string is space $text]} {
    	return
    }
    translate::text $text
}

proc translate::elementstart {gi attspecs} {
    variable transpec
    lpd::elementstart $transpec(spec) $gi $attspecs
    uplevel #0 $transpec(startAction)
    translate::output [uplevel #0 [list subst $transpec(prefix)]]
}

proc translate::elementend {gi} {
    variable transpec
    translate::output [uplevel #0 [list subst $transpec(suffix)]]
    uplevel #0 $transpec(endAction)
    set postAction $transpec(postAction)
    lpd::elementend $transpec(spec) $gi
    uplevel #0 $postAction
}

# translate::lpd spec --
#	Configure LPD $spec to be used as a translate driver.
#
proc translate::lpd {spec {custom {}}} {
    lpd::attlist $spec {
	outputProc	#INHERIT ::translate::write
	stringmap	#INHERIT identity
	diversion	#INHERIT default
    	prefix		#DEFAULT ""
	suffix		#DEFAULT ""
	ignorews	#DEFAULT 0
	startAction	#DEFAULT {}
	endAction	#DEFAULT {}
	postAction	#DEFAULT {}
    }

    lpd::attlist $spec $custom
}

######################################################################
#
# Main driver:
#

proc translate::processFile {spec filename {outputFile stdout}} {
    variable transpec

    lpd::attlist $spec [list outputFile	#INHERIT $outputFile]
    lpd::configure $spec -variable ::translate::transpec
    uplevel #0 [list upvar #0 ::translate::transpec $spec]
    set transpec(sourceFile) $filename
    set transpec(spec) $spec
    lpd::begin $spec

    # Process the file:
    #
    set parser [xml::parser \
    	-characterdatacommand	translate::characterdata \
    	-elementstartcommand	translate::elementstart  \
    	-elementendcommand 	translate::elementend    \
	-baseurl 		"file:[file join [pwd] $filename]" \
    ]
    # -externalentitycommand	sgml::ResolveEntity
    $parser parse [read [set fp [open $filename r]]]
    close $fp
    rename $parser {}

    # Finish up:
    #
    translate::flush
    translate::writeln ""
    lpd::end $spec
}

proc translate::processSubdocument {spec filename} {
    set parser [xml::parser \
    	-characterdatacommand	translate::characterdata \
    	-elementstartcommand	translate::elementstart  \
    	-elementendcommand 	translate::elementend    \
	-baseurl 		"file:[file join [pwd] $filename]" \
    ]
    # -externalentitycommand	sgml::ResolveEntity
    $parser parse [read [set fp [open $filename r]]]
    close $fp
    rename $parser {}
}

#*EOF*

