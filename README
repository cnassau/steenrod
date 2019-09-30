# The Steenrod Tcl library

This repository contains the author's C-code for efficient manipulation
of Steenrod algebra related structures; this includes enumeration of Steenrod
algebra elements and optimized multiplication routines. All computations
take place in the Milnor basis. The library is meant to be used through its
Tcl interface; its primary purpose is to function as the main "engine"
underlying the author's [Sage/Yacop](https://github.com/cnassau/yacop-sage) project.

## Building the library

* Make sure you have Tcl headers and development libraries installed. On Ubuntu
you need, for example, to install the tcl-dev package - other distributions will
offer similar packages. For the graphical demo resolver you also need the "tk"
package with its "wish" executable.

* To build, create a subdirectory "build" and cd into it. Then run "../configure", "make", and "sudo make install"

* After running "make install" you can run "make bigtest" to run some consistency tests.

## Running the demo resolver

Change to directory demo. Then try some of the following command lines

  wish ./resolver.tcl -prime 3 -algebra '0 -1 {9 9 9 9 9 9} 0' -maxdim 80 -maxs 25
  wish ./resolver.tcl -prime 2 -algebra '0 0 {9 9 9 9 9 9} 0' -maxdim 80 -maxs 25 -viewtype even
  wish ./resolver.tcl -prime 2 -algebra '0 0 {3 2 1} 0' -maxdim 80 -maxs 25 -viewtype even

The algebra format deserves some explanation: the algebra is specified as
a list of 4 entries, 2 of which are actually ignored. The meaning of the 4 constituents
is this:

    "0"          coefficient (ignored)
    "-1"         bitmask, specifying the Bocksteins (all in this case)
    "9 9 9 9 9"  profile of the reduced part of the subalgebra     
    "0"          generator id (ignored)

The resolutions are recomputed from scratch on every run, nothing is saved.

The status line of the resolver shows
  - the prime
  - the subalgebra A that is being resolved
  - an auxiliary subalgebra B that is used to divide the computation into smaller pieces 
- - the dimension of the matrices for the initial homology calculation
  - the dimension of the matrix involved in the current lifting problem

There is also a non-graphical version that runs when you specify "-usegui 0".
