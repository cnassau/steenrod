# [incr Tcl] package for the Steenrod algebras 
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

load libtprofile.so

# PROFILES --------------------------------------------------------------------

class profile {
    
    private variable pi     ;# primeInfo object 
    private variable prof   ;# the underlying profile object
    private variable core   ;# prof's core object

    constructor {} {
        
    }


}

# TWO ALGEBRA ENVIRONMENT -----------------------------------------------------

class algpair {

    private variable pi     ;# primeInfo object 
    private variable enenv  ;# the underlying enumEnv object 

}

# MILNOR BASIS ELEMENTS -------------------------------------------------------

class milnor {

    private variable exmon   ;# the underlying extended monomial

}




# make this the last instruction to ensure that errors invalidate the package  
package provide steenrod 1.0
