#!/bin/sh
######################################################################
# $Id$
# Created: 20 Jun 2002
# Copyright (C) 2002, Joe English.  All rights reserved.
######################################################################
# \
    exec tclsh $0 "$@"
#
# Description:
#	Convert TMML manpages to HTML.
#

package require cmdline

set libdir [file dirname [info script]]
source [file join $libdir lpd.tcl]
source [file join $libdir translate.tcl]

variable optlist {
    {output.arg		"-" 	"Write output to specified file" }
    {verbose			"Be verbose?" }
    {debuglpd			"Debug LPD?" }
}

######################################################################
#
# HTML output routines:
#

namespace eval html { }

# html::startTag gi [ attspecs ... ]
#
#  Emit a start-tag for element 'gi'.
#
#  'attspecs...' is a paired list of attribute-name/attribute-value pairs.
#
variable html::attvalEscape {
	{&} 	&amp;
	{<} 	&lt;
	{>} 	&gt;
	{"} 	&quot;
	{'} 	&squot;
}

# " (for emacs)

proc html::startTag {gi args} {
    variable attvalEscape
    if {[llength $args] == 1} { set args [lindex $args 0] }

    if {[llength $args] % 2} {
    	return -code error "Odd number of attribute-value pairs: $gi $args"
    }
    set tag "<$gi"
    foreach {attname attval} $args {
	append tag " $attname=\"[string map $attvalEscape $attval]\""
    }
    append tag "\n>"
    translate::output $tag
}

# emptyElement --
#	Same as startTag.
#
interp alias {} html::emptyElement {} html::startTag

# html::endTag gi
#
#	Emit an end-tag for element 'gi',
#	unless end-tag omission is specified for element type 'gi'.
#
variable html::omitEnd {
    BR AREA LINK IMG PARAM HR INPUT COL META
    FRAME ISINDEX BASE
}
# ALSO: P DT DD LI
proc html::endTag {gi} {
    variable omitEnd
    if {[lsearch $omitEnd [string toupper $gi]] < 0} {
	translate::output "</$gi>"
    }
}

# html::element  gi ?attname attval...? script
#
# 	Convenience function: Emit start tag, evaluate script,
#	then emit end tag.
#
proc html::element {gi args} {
    set script [lindex $args end]
    set atts [lreplace $args end end]
    if {[llength $atts] == 1} { set atts [lindex $atts 0] }
    html::startTag $gi $atts
    uplevel 1 $script
    html::endTag $gi
}

interp alias {} html::text {} translate::text

######################################################################
#
# Misc. junk:
#

# NOTE 'div class=table': without a DIV wrapper, Netscape misformats tables
#
proc startTable {headers} {
    html::startTag div class table
    html::startTag table width 100% rules none cellpadding 5%
    html::element thead {
	set ::tmml2html(ncols) 0
	html::element tr class heading {
	    foreach {text width} $headers {
		html::element th width $width { html::text $text }
		incr ::tmml2html(ncols)
	    }
	}
    }
}
proc endTable {} {
    html::endTag table 
    html::endTag div
}

######################################################################
#
# Specification:
#

translate::lpd tmml2html {
    result #DEFAULT #IMPLIED
    startAction #DEFAULT {
	set result [subst $tmml2html(result)]
    	switch -- [set tmml2html(resultGI) [lindex $result 0]] {
	    #IMPLIED  {}
	    default {
	    	html::startTag $tmml2html(resultGI) [lrange $result 1 end]
	    }
	}
    }
    endAction #DEFAULT {
    	if {![string equal $tmml2html(resultGI) #IMPLIED]} {
	    html::endTag $tmml2html(resultGI)
	}
    }
}

translate::stringmap html { & &amp;  < &lt;  > &gt; }

variable pubid	"-//W3C//DTD HTML 4.0 Transitional//EN"
variable generator [file tail [info script]]

lpd::linkset tmml2html #INITIAL {
    manpage {
	stringmap html
    	startAction {
	    translate::output "<!DOCTYPE HTML PUBLIC \"$::pubid\">\n"
	    html::startTag html
	    html::element head {
	    	html::element title {
		    append tmml2html(@title) ""	;# %%% make sure it exists
		    html::text $tmml2html(@title)
		}
		html::emptyElement link \
		    rel stylesheet type text/css href steenman.css
		html::emptyElement meta name generator content $::generator
	    }
	    html::startTag body
	    # @@@ HERE: navbar
	    html::startTag div class body
	}
	endAction {
	    html::endTag div
	    html::endTag body
	    html::endTag html
	}
    }
}

lpd::linkset tmml2html #INITIAL {
    head { result #IMPLIED #USE head }
}
lpd::linkset tmml2html head {
    info { result #IMPLIED }
    link { result #IMPLIED }
}

# Header and footer sections:
#
lpd::linkset tmml2html #INITIAL {
    namesection {
	#USE namesection
    	startAction {
	    html::element h2 { html::text NAME }
	    set ::tmml2html(names) [list]
	}
	endAction {
	    html::element p class namesection {
		html::element b class names {
		    translate::output [join $::tmml2html(names) ", "]
		}
		html::text " - "
		#html::emptyElement br
		translate::output [translate::undivert desc]
	    }
	}
    }

    synopsis {
	result #IMPLIED
	startAction { html::element h2 { html::text SYNOPSIS } }
	endAction { }
    }

    keywords {
    	result #IMPLIED  #USE kwseealso 
	startAction startkwseealso endAction endkwseealso
	title "KEYWORDS"
    }
    seealso {
    	result #IMPLIED  #USE kwseealso 
	startAction startkwseealso endAction endkwseealso
	title "SEE ALSO"
    }
}

lpd::linkset tmml2html namesection {
    name {
    	diversion name  result #IMPLIED
	postAction { lappend ::tmml2html(names) [translate::undivert name] }
    }
    desc {
    	diversion desc result #IMPLIED
    }
}

proc startkwseealso {} {
    html::element h2 { html::text $::tmml2html(title) } 
    set ::tmml2html(words) [list]
}
proc endkwseealso {} {
    html::element p {
	translate::output [join $::tmml2html(words) ", "]
    }
}
lpd::linkset tmml2html kwseealso {
    {keyword ref uri} { 
	result #IMPLIED diversion word stringmap html
	postAction { lappend ::tmml2html(words) [translate::undivert word] }
    }
}

# Other sections:
#
lpd::linkset tmml2html #INITIAL {
    section 	{ result { div class section }  	#USE section }
    subsection	{ result { div class subsection }	#USE subsection }
}
lpd::linkset tmml2html section		{ title	{ result h2 } }
lpd::linkset tmml2html subsection	{ title	{ result h3 } }

# HTML-like elements:
#
lpd::linkset tmml2html #INITIAL {
    p	{ result p }
    ul	{ result ul }
    ol 	{ result ol }
    li	{ result li }
    dl	{ result dl }
    dle	{ result #IMPLIED }
    dt	{ result dt }
    dd	{ result dd }

    br	{ result br }

    syntax	{ result { pre class syntax } }
    example	{ result { pre class example } }

    emph	{ result em }
    i		{ result i }
    b		{ result b }

    url	{
    	result #IMPLIED diversion url startAction { }
	endAction {
	    set url [translate::undivert url]
	    html::element a href $url { html::text "<$url>" }
        }
        postAction {
            set rtext [translate::undivert url]
            translate::output $rtext
        }
    }
}

# Syntax elements:
#
lpd::linkset tmml2html #INITIAL {
    {	term cmd variable method option file
	syscmd fun widget package type class
    } {
    	result {b class term}
    }
    m 		{ result {i class m} }
    o		{ result #IMPLIED  prefix " ?" suffix "? " }
    l		{ result tt }
    samp	{ result tt }
}

# Special-purpose lists:
#
lpd::linkset tmml2html #INITIAL {
    optlist	{ result dl #USE optlist }
    commandlist	{ result dl #USE commandlist }
    arglist {
    	result #IMPLIED #USE arglist
	startAction { startTable { Type 20%  Name 70%  Mode 10% } }
	endAction   { endTable }
    }
    optionlist {
    	result #IMPLIED #USE arglist
	startAction {
	    startTable { "Name" 20% "Database name" 40% "Database class" 40% }
	}
	endAction	{ endTable }
    }
}

lpd::linkset tmml2html optlist {
    optdef	{ result #IMPLIED }
    optname 	{
    	result 		#IMPLIED 
	startAction 	{ html::startTag dt }
	endAction	{ }
	suffix		{ }
    }
    optarg	{ result {i class arg} }
    desc	{ result dd }
}

lpd::linkset tmml2html commandlist {
    commanddef	{ result #IMPLIED }
    command	{ result dt }
    desc	{ result dd }
}

lpd::linkset tmml2html arglist {
    {argdef optiondef} {
    	result #IMPLIED
	startAction { html::startTag tr class syntax }
	endAction {}
    }
    {argtype name argmode}	{ result td }
    {dbname dbclass}		{ result td }
    desc {
	result #IMPLIED
	startAction {
	    html::endTag tr
	    html::startTag tr class desc
	    html::element td class padding { translate::output "&nbsp;" }
	    html::startTag td \
	    	colspan [expr {$::tmml2html(ncols) - 1}]
	}
	endAction {
	    html::endTag td
	    html::endTag tr
	}
    }
}

# Simple lists:
# (@@@TBD: extended lists)

lpd::linkset tmml2html #INITIAL {
    sl	{
	result #IMPLIED  #USE sl
	@cols 2
	startAction {
	    set ::tmml2html(tdwidth) [expr {floor(100 / $::tmml2html(@cols))}]
	    set ::tmml2html(licounter) 0

	    html::startTag div class menu
	    html::startTag table class menu width 100%
	}
	endAction {
	    html::endTag table ; html::endTag div
	}
    }
}
lpd::linkset tmml2html sl {
    li {
	result #IMPLIED
	startAction {
	    if {$::tmml2html(licounter) % $::tmml2html(@cols) == 0} {
	    	html::startTag tr
	    }
	    html::startTag td width "$::tmml2html(tdwidth)%"
	}
	endAction {
	    html::endTag td
	}
	postAction {
	    if {[incr ::tmml2html(licounter)] % $::tmml2html(@cols) == 0} {
	    	html::endTag tr
	    }
	}
    }
}

######################################################################
#
# Main routine:
#
proc main {} {
    variable options
    array set options [cmdline::getoptions ::argv $::optlist]
    if {$options(debuglpd)} {
	lpd::configure tmml2html -debug 1
    }
    global argv
    if {[llength $argv] != 1} {
	puts stderr [cmdline::usage $::optlist]
	exit 1
    }
    set filename [lindex $argv 0]
    if {$options(output) == "-"} {
    	set output stdout
    } else {
    	set output [open $options(output) w]
    }
    translate::processFile tmml2html $filename $output
}

if {![string compare $::argv0 [info script]]} { main }

#*EOF*

