#!/usr/bin/tclsh
#
# Copy library files to platform specific library path  
#
# Copyright (C) 2004 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

set targdir [file join .. lib $tcl_platform(os)]

if {[llength $argv] < 1} {
    puts "use \"$argv0 <file1> <file2> ... \" to copy files into $targdir"
    exit 1
}

foreach fname $argv {

    if {![file readable $fname]} {
        puts "Can't read file $fname"
        exit 1
    }

    if {![file isdirectory $targdir]} {
        file mkdir $targdir
    }

    set newname [file tail $fname]
    if {[file extension $newname] == ".so"} {
        # map this to native shared lib extension
        set newname [file rootname $newname][info sharedlibextension]
    }

    set target [file join $targdir $newname]
    # puts "file copy -force $fname $target"
    file copy -force $fname $target
    #file delete $fname 
}
