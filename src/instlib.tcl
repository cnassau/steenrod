#!/usr/bin/tclsh
#
# Move (library) file to platform specific library path  
#
# Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

set targdir [file join .. lib $tcl_platform(os)]

if {[llength $argv] != 1} {
    puts "use \"$argv0 <file>\" to move file into $targdir"
    exit 1
}

set fname [lindex $argv 0]

if {![file readable $fname]} {
    puts "Can't read file $fname"
    exit 1
}

if {![file isdirectory $targdir]} {
    file mkdir $targdir
}

file copy -force $fname $targdir
file delete $fname 
