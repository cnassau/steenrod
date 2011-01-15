set dir .
source pkgIndex.tcl
package req Steenrod
steenrod::cl::impl::init
puts $steenrod::cl::_info
steenrod::cl::impl::combi program {

    __kernel void memset0(__global uint4 *mem) {
	mem[get_global_id(0)] = 0;
    }

    /* we can't write to a char* without this pragma */
    #pragma OPENCL EXTENSION cl_khr_byte_addressable_store : enable


    __kernel void multffp(__constant int *seqinfo,
			  __constant short *multmatrix,
			  __global unsigned char *outmatrix,
			  int rowoffset,
			  __constant short16 *dg,
			  short mx0,
			  short mx1,
			  short mx2,
			  short mx3,
			  short mx4,
			  short mx5,
			  short mx6,
			  short mx7,
			  short mx8,
			  short mx9,
			  short mx10,
			  short mx11,
			  short mx12,
			  short mx13,
			  short mx14,
			  short mx15,
			  short esum,
			  short emsk,
			  short coeff
			  ) {
			      const int idx = get_global_id(0);

			      //outmatrix[rowoffset+idx] = coeff;
			  }

}

set testsz 368
#set testsz 268
set testsz 168
#set testsz 35


proc runtest {enmconfig} {
    steenrod::enumerator A
    foreach itm $enmconfig {
        array set cfg $itm
        A configure {*}$itm
    }
    #parray cfg

    steenrod::monomap d
    set cnt 0
    set gl {}
    set gcnt -1
    foreach m [A basis] {
	if {($cnt&0x7ff)==0} {
	    incr gcnt
	    lappend gl [list $gcnt 0 0]
	}
	lset m end $gcnt
	d add [list 1 0 {} 0] [list $m]
	incr cnt
    }
    #puts [join [d list] \n]
    
    steenrod::enumerator B -prime $cfg(-prime) -algebra $cfg(-algebra) -genlist $gl

    set opideg $cfg(-ideg)
    set opedeg $cfg(-edeg)
    set p $cfg(-prime)
    set dmi $opideg
    set dme $opedeg
    set off [expr {2*($p-1)}]

    set cfg(time-cpu) 0.0
    set cfg(time-gpu) 0.0


    set xcnt 0
    while {[incr xcnt]<5} {
	A configure -ideg $dmi -edeg $dme
	B configure -ideg [expr {$dmi+$opideg}] -edeg [expr {$dme+$opedeg}]
	#puts A=[A basis]\nB=[B basis]

	lappend cfg(dimensions) [A dim]x[B dim]

	foreach mode {gpu cpu} {
	    set ::steenrod::useOpenCL [expr {($mode == "gpu") ? 1 : 0}]
	    set msecs [lindex [time {      
		steenrod::ComputeMatrix A d B
	    } 4] 0]
	    set cfg(time-$mode) [expr {$cfg(time-$mode)+$msecs}]
	    set res [steenrod::ComputeMatrix A d B]
	    #puts [join $res \n]
	    puts $mode:$xcnt:[steenrod::matrix dimensions $res];#:[string range $res 0 60]...
	    unset res
	    #puts [steenrod::matrix dimensions $res]
	}
	incr dmi $off 
    }

    parray cfg
}


steenrod::enumerator A 
foreach p {3 2 5 7} {
 set enmlist {}
 set off [expr {2*($p-1)}]
 for {set dim 0} {$enmlist == "" && $dim<2*($p**7)} {incr dim $off} {
  for {set e 0} {$e<1} {incr e} {
   A configure -prime $p -edeg $e -ideg $dim -genlist {{0 0 0}}

   if {[A dimension]>$::testsz} {
    lappend enmlist [A configure]
    break
   }
  }
 }
 runtest [lindex $enmlist 0]
   # break
}



