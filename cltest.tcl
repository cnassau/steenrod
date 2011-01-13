set dir .
source pkgIndex.tcl
package req Steenrod
steenrod::cl::impl::init
puts $steenrod::cl::_info
steenrod::cl::impl::combi program {
    void f() {};
//uuh

__kernel void do4711(__global unsigned char *out, char val) {
   *out = val;
}

    typedef struct {
       short rdat[8];
       int   edat;
       int   id;
    } exmo;

    __kernel void pipeek(__constant char *pi,
                         __global int *out,
                         __constant exmo *srcbas,
                         int srcdim, int dstdim, int bytesperrow,
                         __global unsigned char *outmat) {
         const int idx = get_global_id(0);
#if 0
       int srcdim = outmat[0];
       int dstdim = *(outmat+1);
       const int bytesperrow = outmat[2];
#endif
       __global unsigned char *data = outmat;
       int i,cnt=0;
         for(i=0;i<dstdim;i++)
           do4711(data+idx*bytesperrow+i,idx+i);

       cnt=0;
       int val=srcdim;
       //for(;cnt<8;) {do4711(data + cnt++,val&0xff); val>>=8;}
    }

}

set testsz 20


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
    foreach m [A basis] {
       lappend gl [list $cnt 0 0]
       lset m end $cnt
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

       foreach mode {cpu gpu} {
        set ::steenrod::useOpenCL [expr {($mode == "gpu") ? 1 : 0}]
        set msecs [lindex [time {      
           steenrod::ComputeMatrix A d B
        } 4] 0]
        set cfg(time-$mode) [expr {$cfg(time-$mode)+$msecs}]
        set res [steenrod::ComputeMatrix A d B]
        #puts [join $res \n]
        puts $mode:$res
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
}



