#!/usr/bin/tclsh
#
# Load the platform specific version of the Steenrod algebra library
#
# Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

set libdir [file join [file dirname [info script]] $tcl_platform(os)]

if {![file isdirectory $libdir]} {
    error "Did not find Steenrod library for platform '$tcl_platform(os)'"
}

load [file join $libdir libsteenrod[info sharedlibextension]]

package provide Steenrod 1.0
