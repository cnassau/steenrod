######################################################################
# $Id$
# Created: 9 Aug 2000
# Copyright (C) 2000, Joe English.  All rights reserved.
######################################################################
#
# Description:
#
#	XML processor inspired by the SGML "IMPLICIT LINK" feature.
#
#	An LPD is an event-driven processor which associates
#	"link attributes" with the current element based
#	on the element type names and it's context.  Link attributes
#	can be stylesheet entries, snippets of Tcl code, or anything else.
#
#	The LPD processor maintains the current set of link
#	attribute values in a global array.  At the start of an
#	element new settings are computed based on the current link
#	set stack, and at the end of each element the previous
#	values are restored In addition to the link attributes,
#	all of the "regular" attributes of the current element are
#	also available as keys with the attribute name prefixed
#	with an '@'.  Application code may also set keys in the
#	LPD array variable; the previous values will be restored
#	at the end of the current element.
#
#	A "link set" is a named collection of "link rules";
#	each link rule maps element type names to a list
#	of name/value pairs specifying link attribute
#	values.
#
#	The processor maintains a stack of link sets during
#	processing.  The link attribute '#USE' specifies
#	the name of a link set which will be pushed on the
#	stack while the element is active; '#POST' specifies the name
#	of a link set to be used *after* the elemnt is finished.
#
# TODO: check #USE and #POST rules to make sure ruleset is defined

######################################################################
#
# Configuration:
#

namespace eval lpd {
    variable LPD		;# map lpd.linkrule.gi -> paired list
    variable LPDConf
    variable LPDState
    array set LPDOptions {
	-variable		::LPD
	-debug 			0
	-warnunrecognized	1
    }
}

# lpd::initialize 'lpd' --
#	Set default values for 'lpd' iff it has not yet been initialized
#
proc lpd::initialize {lpd} {
    variable LPDConf
    if {![info exists LPDConf($lpd)]} {
	variable LPDOptions
	foreach {option value} [array get LPDOptions] {
	    set LPDConf($lpd.$option) $value
	}
	set LPDConf($lpd.defaults) [list #USE {} #POST {}]
	set LPDConf($lpd.srcatts) [list]
    }
    set LPDConf($lpd) 1
}

# lpd::configure --
#	Set LPD options; see variable LPDOptions for the legal options
# 
proc lpd::configure {lpd args} {
    variable LPDConf
    variable LPDOptions

    lpd::initialize $lpd
    foreach {option value} $args {
    	if {![info exists LPDOptions($option)]} {
	    return -code error [concat \
	    	"Bad option $option\n" \
	    	"must be one of: [join [array names LPDOptions] {, }]" ]
	}
	set opts($option) $value
    }
    foreach {option value} [array get opts] {
    	set LPDConf($lpd.$option) $value
    }
}

######################################################################
#
# Declarations:
#

# lpd::attlist lpd {attname attdefault value... } --
#	Declare link attributes and their initial/default values.
#	'attdefault' specifies what value to use for link attributes
#	that are not specified in any active link rule:
#		#DEFAULT -- use 'value'
#		#INHERIT -- use link attribute value from the parent element;
#	Declaring 
#
# %%% TODO: check for duplicates
#
proc lpd::attlist {lpd attspecs} {
    variable LPDConf
    lpd::initialize $lpd
    foreach {attname attdef attval} $attspecs {
    switch -- $attdef {
	"#INHERIT" {
	    lappend LPDConf($lpd.initial) $attname $attval
	}
	"#DEFAULT" {
	    lappend LPDConf($lpd.initial) $attname $attval
	    lappend LPDConf($lpd.defaults) $attname $attval
	}
	default {
	    return -code error "Unrecognized link attribute default $attdef"
	}
    }}
}

proc lpd::linkset {lpd linkset linkrules} {
    variable LPD
    lpd::initialize $lpd
    foreach {gis linkrule} $linkrules {
	foreach gi $gis {
	    if {[info exists LPD($lpd.$linkset.$gi)]} {
	    	puts stderr "Warning $lpd.$linkset.$gi being redefined"
	    }
	    set LPD($lpd.$linkset.$gi) $linkrule
	}
    }
}

######################################################################
#
# Processing:
#

proc lpd::begin {lpd} {
    variable LPDState
    variable LPDConf
    variable LPDDefaults
    set LPDState($lpd.depth) 0
    set LPDState($lpd.linkrules) [list #INITIAL]
    upvar #0 $LPDConf($lpd.-variable) statevar
    array set statevar $LPDConf($lpd.initial)
}

proc lpd::end {lpd} {
    #; no-op
}

proc lpd::elementstart {lpd gi attspecs} {
    variable LPDState
    variable LPDConf
    variable LPD
    upvar #0 $LPDConf($lpd.-variable) statevar
    set LPDState($lpd.settings.[incr LPDState($lpd.depth)]) [array get statevar]
    array set statevar $LPDConf($lpd.defaults)
    set found 0
    foreach linkrule $LPDState($lpd.linkrules) {
    	if {[info exists LPD($lpd.$linkrule.$gi)]} {
	    array set statevar $LPD($lpd.$linkrule.$gi)
	    set found 1
	}
    }
    if {!$found && $LPDConf($lpd.-warnunrecognized)} {
	puts stderr "Warning: $lpd: Unrecognized element name <$gi>"
	puts stderr "   rulesets: [join $LPDState($lpd.linkrules) /]"
	# Only trigger warning once for this link rule:
	set LPD($lpd.$linkrule.$gi) [list]
    }
    foreach {attname attval} $attspecs {
	set statevar(@$attname) $attval
    }
    if {$LPDConf($lpd.-debug)} {
	set msg "$lpd/$gi: "
    	foreach key [array names statevar] {
	    append msg " $key=<$statevar($key)>"
	}
	puts stderr $msg
    }

    if {[string length [set uselink $statevar(#USE)]]} {
	lappend LPDState($lpd.linkrules) $uselink
	set statevar(#POPLINK) 1
    } else {
    	set statevar(#POPLINK) 0
    }
}

proc lpd::elementend {lpd gi} {
    variable LPDState
    variable LPDConf
    variable LPD
    upvar #0 $LPDConf($lpd.-variable) statevar
    if {$statevar(#POPLINK)} {
    	set LPDState($lpd.linkrules) [lrange $LPDState($lpd.linkrules) 0 end-1]
    }
    set postlink $statevar(#POST)
    catch { unset statevar }
    array set statevar $LPDState($lpd.settings.$LPDState($lpd.depth))
    incr LPDState($lpd.depth) -1

    if {[string length $postlink]} {
    	if {$statevar(#POPLINK)} {
	    set LPDState($lpd.linkrules) \
	    	[lreplace $LPDState($lpd.linkrules) end end $postlink]
	} else {
	    lappend LPDState($lpd.linkrules) $postlink
	    set statevar(#POPLINK) 1
	}
    }
}

#*EOF*

