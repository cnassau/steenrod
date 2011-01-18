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


  
#ifndef __ENDIAN_LITTLE__
   unchecked!
#endif

    typedef struct {
       int id;
       int ext;
       int rrideg;
       int seqoff;
    } effgen;


    int seqoffp(__constant effgen *efflist, int efflen, int id, int ext, int *rrideg) {
        __constant effgen *bot = efflist, *top = efflist + efflen, *wrk2 = bot + efflen/2, *wrk = 0;
        int cr, dummy=0;
#if 1
for(cr=0;cr<efflen;cr++)
if(efflist[cr].id==id && efflist[cr].ext==ext) { *rrideg = efflist[cr].rrideg; return efflist[cr].seqoff; };
return -1;
#else
do {
           wrk = wrk2;
           if(wrk->ext != ext) { cr = ext - wrk->ext; } else { cr = id - wrk->id; };
           if(0==cr) {
              *rrideg = wrk->rrideg;
              return wrk->seqoff;
           } 
           if(0<cr) {
              wrk2 = wrk + (top-wrk)/2;
           } else {
              wrk2 = bot + (wrk-bot)/2;
           }
        } while (wrk != wrk2 && (++dummy<10000));
        return -1;
#endif
    } 
 
    int seqnop(__constant int *constinfo, short16 stop, int g) {
	__constant int    *seqinfo = constinfo + *constinfo; 
        __constant int    *algebra = seqinfo+1;	
        __constant int    *profile = seqinfo+15;
        __constant int    *dimtab  = seqinfo+30;
        __constant int    *seqtab  = seqinfo+45;
        __constant effgen *efflist = (__constant effgen *) (seqinfo+seqinfo[60]);
        const int efflen           = seqinfo[61];

        int effrrideg;
        int soff = seqoffp(efflist,efflen,g,stop.sd,&effrrideg);
        if(soff<0) return -1;
return soff;
	
	return 9;
	return stop.s0;
    }

    void addChars4p(__global uint *mem, 
		    uchar c0, uchar c1, uchar c2, uchar c3, int p) {
        union smd {
           uint i;
           uchar4 c;
        } smd, effsmd, oval, have, havep, want, aux;
        int ok;

        int dummy=0;
 
        smd.c.s0 = c0;
        smd.c.s1 = c1;
        smd.c.s2 = c2;
        smd.c.s3 = c3;

        smd.c %= p;

	do {
	    uchar u;
	    oval.i = atomic_add(mem,smd.i);
	    want.c = ((oval.c % p) + smd.c) % p;
	    have.i = oval.i + smd.i;
	    havep.c = have.c % p;
	    ok = (want.i == havep.i);
	    if (!ok) {
                smd.c.s0 = (want.c.s0 + (p- havep.c.s0));
                smd.c.s1 = (want.c.s1 + (p- havep.c.s1));
                smd.c.s2 = (want.c.s2 + (p- havep.c.s2));
                smd.c.s3 = (want.c.s3 + (p- havep.c.s3));
                smd.c %= p;
	    }
        } while ( !ok && (++dummy<1000000) );
    }

    void setError(__global int *outvars, short16 stop, int gen, int errorcode) {
        int oldval = atom_inc(outvars+19);
        if(oldval != 0) {
           // hack, because atom_xchg is not available on my machine
           atom_dec(outvars+19);
           return;
        }
        outvars[0] = errorcode;
        outvars[1] = gen;
        outvars[2] = stop.sd;
        outvars[3] = stop.s0;
        outvars[4] = stop.s1;
        outvars[5] = stop.s2;
        outvars[6] = stop.s3;
        outvars[7] = stop.s4;
        outvars[8] = stop.s5;
        outvars[9] = stop.s6;
        outvars[10] = stop.s7;
    }

    __kernel void multffp(__constant int *seqinfo,
			  __constant short *multmatrix,
			  __global unsigned char *outmatrix,
			  int bytesperrow,
			  int rowoffset,
			  __global short16 *ffarray,
			  __global short16 *sfarray,
			  __global int *outvars)
    {
	short16 ff = ffarray[get_global_id(0)];
	short16 sf = sfarray[get_global_id(1)];
	const int row = rowoffset + get_global_id(0);
	const int gen = sf.sf | ((int) sf.se) << 16;

	__constant int *enumerator = seqinfo + *seqinfo; 
	__constant int *pinfo = seqinfo;
	const int p = pinfo[1];
	const int NALG = pinfo[2];


#if 0
if ((get_global_id(1) > 0) || (get_global_id(0) > 0) ) return;
#endif


	/* compute ff * sf and store result in outmatrix[bytesperrow*row+*] */

	short16 smd = sf;
	int coeff = 1;
	int sqno = seqnop(seqinfo,smd,gen);
	
        if(sqno>=0) {
	    const int idx = bytesperrow*row+sqno;
	    const int idx2 = idx / sizeof(int);
	    const int off = idx % sizeof(int);

            addChars4p( ((__global uint *) outmatrix) + idx2,
                      (0==off) ? coeff : 0,
                      (1==off) ? coeff : 0,
                      (2==off) ? coeff : 0,
                      (3==off) ? coeff : 0,
                      p);
	} else {
            setError(outvars,smd,gen,1);
        }


 }

}

set testsz 1368
#set testsz 768
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
unset -nocomplain glcnt
    foreach m [A basis] {
	if {($cnt&0x1ff)==0} {
	    incr gcnt
	    lappend gl [list $gcnt 0 0]
	}
incr glcnt($gcnt)
	lset m end $gcnt
	d add [list 1 0 {} 0] [list $m]
	incr cnt
    }
parray glcnt
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



