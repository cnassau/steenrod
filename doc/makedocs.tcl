#!/usr/bin/tclsh
#
# Create html, tmml, and manpage from *.msrc
#
# Copyright (C) 2009 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

lappend auto_path  . doc /usr/share/tcl
lappend auto_path /home/cn/ActiveTcl8.4.3.0-linux-ix86/lib/

package require doctools

proc preformat {data} {
    foreach {kw text} {
        %crossrefs% "enumerator linalg matrix vector mono monomap poly prime"
        %keywords%  "Mathematics {Steenrod algebra} {Tcl package}"
        %copyright% "{2004-2009 Christian Nassau}"
        %homepage%  "http://www.nullhomotopie.de/"
    } {
        set data [regsub -all $kw $data $text]
    }
    return $data
}

# hack: we 'cd html' because '-format html' would otherwise 
# get confused by the directory 'html'  
set cwd [pwd]
cd [file dirname [info script]]
cd html
::doctools::new 2html -format html 
::doctools::new 2man  -format nroff
::doctools::new 2tmml -format tmml
cd ..

set flst [glob -nocomplain *.msrc]

puts "Converting documentation, file pattern = *.msrc"

lappend replmap STEENRODVERSION $::env(PACKAGE_VERSION)
interp alias {} repl {} string map $replmap

foreach name $flst {

    puts "-> $name"

    foreach fmt {2html 2man} { $fmt configure -file $name }

    set inp [open $name]
    set data [read $inp]
    close $inp

    set data [preformat $data]

    set rname [file rootname $name]
    
    set mname [file join man man3 $rname.3steenrod]
    set ch [open aux.n w]
    puts $ch [repl [2man format $data]]
    close $ch
    exec sed -e "/man\.macros/r man.macros" -e "/man\.macros/d" aux.n > $mname
    file delete aux.n

    set ch [open [file join tmml $rname.tmml] w]
    puts $ch [repl [2tmml format $data]]
    close $ch
    
    if 1 {
        set ch [open [file join html $rname.html] w]
        puts $ch [repl [2html format $data]]
        close $ch
    } else {
        exec tclsh tmml-html.tcl [file join tmml $rname.tmml] > [file join html $rname.html]
    }
}

