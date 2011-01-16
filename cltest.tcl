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

    #pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable 

    void GetSemaphor(__global int *semaphor) {
	int occupied = atomic_xchg(semaphor, 1);
	while(occupied > 0)
	{
	    occupied = atomic_xchg(semaphor, 1);
	}
    }
    void ReleaseSemaphor(__global int *semaphor)
    {
	int prevVal = atomic_xchg(semaphor, 0);
    }
    
    int seqnop(__constant int *seqinfo, short16 stop, int g) {
	
	return 9;
	return stop.s0;
    }

    __kernel void multffp(__constant int *seqinfo,
			  __constant short *multmatrix,
			  __global unsigned char *outmatrix,
			  int bytesperrow,
			  int rowoffset,
			  __global short16 *ffarray,
			  __global short16 *sfarray,
			  __global int *semaphores,
			  int nsemaphores)
    {
	short16 ff = ffarray[get_global_id(0)];
	short16 sf = sfarray[get_global_id(1)];
	const int row = rowoffset + get_global_id(0);
	const int gen = sf.sf | ((int) sf.se) << 16;

	const int p = 3;

	/* compute ff * sf and store result 
	** in outmatrix[bytesperrow*row+*] */

	short16 smd = sf;
	int coeff = 1;
	int sqno = seqnop(seqinfo,smd,gen);
	if(sqno>=0) {
	    const int idx = bytesperrow*row+sqno;
	    __global int *aux = (__global int *)outmatrix;
	    const int idx2 = idx / sizeof(int);
	    const int off = idx % sizeof(int);
	    int oldval = atomic_add(&(aux[idx2]),coeff<<(8*off));
	    atomic_add(&(aux[(bytesperrow*row)/sizeof(int)]),oldval);
	    // todo: inspect oldval and fix overflows + reduce mod p
	}
    }

}

set testsz 1368
#set testsz 368
#set testsz 268
#set testsz 168
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
	if {($cnt&0x1ff)==0} {
	    incr gcnt
	    lappend gl [list $gcnt 0 0]
	}
	lset m end $gcnt
	d add [list 1 0 {} 0] [list $m]
	incr cnt
    }
puts gcnt=$gcnt
    #puts [join [d list] \n]

    steenrod::enumerator B -prime $cfg(-prime) -algebra $cfg(-algebra) -genlist $gl

    set opideg [A cget -ideg]
    set opedeg [A cget -edeg]
    set p $cfg(-prime)
    set dmi $cfg(-ideg)
    set dme $cfg(-edeg)
    set off [expr {2*($p-1)}]

    set cfg(time-cpu) 0.0
    set cfg(time-gpu) 0.0


    set xcnt 0
    while {[incr xcnt]<5} {
	set dmi [set dme 0];incr xcnt 10;puts "--> src trivialised <--"
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
	    puts $mode:$xcnt:[steenrod::matrix dimensions $res]:[string range $res 0 60]...
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
    break
}



