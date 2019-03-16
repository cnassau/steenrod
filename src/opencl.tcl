package provide Steenrod::opencl [package present Steenrod]

namespace eval ::steenrod::cl {

    proc Log {lvl msg} {
        puts [format {%-5s %s} $lvl: $msg]
    }

    proc choose-device {} {
        set pattern .*
        set varname YACOP_OPENCL_DEVICE
        if {[info exists ::env($varname)]} {
            set pattern $::env($varname)
            if {[catch {regexp -- $pattern dummy} errmsg]} {
                error "Error in regular expression \"$pattern\" (from environment variable $varname):\n$errmsg"
            }
        }
        Log debug "Looking for OpenCL device matching '$pattern' (use $varname to override)"
        set answers {}
        set keys {}
        foreach {platformid desc} [platform list] {
            set pkey [dict get $desc CL_PLATFORM_NAME]
            foreach {deviceid devdesc} [dict get $desc devices] {
                set key "$pkey/[dict get $devdesc CL_DEVICE_NAME]"
                Log debug "Available device '$key'"
                lappend keys $key
                if {[regexp -nocase -- $pattern $key]} {
                    lappend answers [list $platformid $deviceid]
                }
            }
        }
        if {0 == [llength $answers]} {
            if {0 == [llength $keys]} {
                error "No OpenCL devices found"
            }
            error "No OpenCL device matches pattern '$pattern' (given by YACOP_OPENCL_DEVICE). Pick one of '[join $keys {', '}]'"
        }
        if {1 < [llength $answers]} {
            error "More than one OpenCL device found. Set YACOP_OPENCL_DEVICE to pick one of '[join $keys {', '}]'"
        }
        platform context [namespace current]::ctx $platformid $deviceid
        ctx setcontext
    }

    proc dump {} {
        foreach d [list [ctx platform] [ctx device]] {
            foreach k [lsort [dict keys $d]] {
                puts [format {%-45s = %s} $k [dict get $d $k]]
            }
        }
    }

    set code "#define NALG $::steenrod::NALG\n"
    append code \n $::steenrod::cl::typedefs

    append code {
        kernel void zeromatrix(global HOST_INT *data, int ipr, int nrows, int ncols) {
            int row = get_global_id(0), col = get_global_id(1);
            if(row>=nrows || 8*sizeof(HOST_INT)*col>=ncols) return;
            int offset = row*ipr + col;
            data[offset] = 0;
        }

        kernel void unitmatrix(global HOST_INT *data, int ipr, int nrows, int ncols) {
            int row = get_global_id(0), col = get_global_id(1);
            if(row>=nrows || 8*sizeof(HOST_INT)*col>=ncols) return;
            int offset = row*ipr + col;
            int rcol = 8*sizeof(HOST_INT)*col;
            HOST_INT val = 0;
            if(rcol <= row && row < rcol+(8*sizeof(HOST_INT))) {
                val = 1;
                val <<= (row-rcol);
            }
            data[offset] = val;
        }
    }

    append code {
        // code to create random matrices with limited ranks, used for testing
        // based on http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
        
        // FIXME: our random matrices should really be independent of sizeof(HOST_INT)

        /* the bitcount macro from the fortune database */
        #define BITCOUNT(x)     (((BX_(x)+(BX_(x)>>4)) & 0x0F0F0F0F) % 255)
        #define  BX_(x)         ((x) - (((x)>>1)&0x77777777)                   \
                                     - (((x)>>2)&0x33333333)                   \
                                     - (((x)>>3)&0x11111111))

        uint wang_hash(uint seed)
        {
            seed = (seed ^ 61) ^ (seed >> 16);
            seed *= 9;
            seed = seed ^ (seed >> 4);
            seed *= 0x27d4eb2d;
            seed = seed ^ (seed >> 15);
            return seed;
        }

        uint random_matrix_coeff(uint row, uint col, int seed, int rounds) {
            uint rdata = wang_hash(row*0xdeadbeef+seed+col);
            while(rounds--) {
                rdata ^= (rdata << 13);
                rdata ^= (rdata >> 17);
                rdata ^= (rdata << 5);
            }
            return rdata;
        }

        kernel void random_matrix(global uint *data, int ipr, int seed, int rounds) {
            uint row = get_global_id(0), col = get_global_id(1);
            uint offset = row*ipr + col;
            data[offset] = random_matrix_coeff(row,col,seed,rounds);
        }

        kernel void rank_random_matrix(global uint *data, 
                                       int nrows, int ncols, int ipr,
                                       uint rank,
                                       int seed, int rounds,
                                       local uint *buffer) {
            uint row = get_global_id(0), col = get_global_id(1);
            uint offset = row*ipr + col;
            int seed2 = 0x34baba17*seed ^ seed;

            uint mask = ~0;
            if((col+1)*32 >= ncols) mask >>= (col+1)*32-ncols;

            // make buffer[j+32*i] = random_matrix_coeff(32*col+j,i,seed2,rounds);

            int idx = get_local_id(0), idxmax = 32*((rank+31)/32);
            for(;idx<idxmax;idx+=get_local_size(0)) {
                uint j = idx & 31;
                uint i = (idx-j) / 32;
                buffer[idx] = random_matrix_coeff(32*col+j,i,seed2,rounds);
            }

            barrier(CLK_LOCAL_MEM_FENCE);

            if(row >= nrows) return;

            uint ans = 0;
            for(int i=0;i<rank;i+=32) {
                uint acoeff = random_matrix_coeff(row,i,seed,rounds);
                if(i+32>=rank) {
                    int nbitstokeep = i+32-rank;
                    uint mask = 1;
                    mask <<= nbitstokeep;
                    mask--;
                    acoeff = acoeff & mask;
                }
                uint bit;
                for(int j=0,bit=1;j<32;j++,bit<<=1) {
                    uint bcoeff = buffer[j+i];
                    uint x = acoeff & bcoeff;
                    if(1 & BITCOUNT(x)) {
                        ans ^= bit;
                    }
                }
            }
            data[offset] = ans & mask;
        }

    }


    foreach size {8 16 32 64} dtp {uchar ushort uint ulong} fmt {%02x %04x %08x %016x} {
        set map [list "\$SIZE" $size "\$DTP" $dtp "%FMT" $fmt]
        append code [string map $map {

            void kbprintbin$SIZE($DTP val,int colcnt) {
                $DTP b=1;
                for(;b;b<<=1) {
                    if(0 == colcnt--) printf("|");
                    printf("%d",(val&b)?1:0);
                }
            }
            void kbprintrow$SIZE(global $DTP *data, int ncols) {
                for(;ncols>0;data++,ncols-=$SIZE) {
                    printf(".");
                    kbprintbin$SIZE(*data,ncols);
                }
                printf("\n");
            }
            void kbprintmat$SIZE(global $DTP *data, int nrows, int ncols, int ipr) {
                for(int i=0;i<nrows;i++,data += ipr) {
                    printf("%3d:",i);
                    kbprintrow$SIZE(data,ncols);
                }
            }

            kernel void kbinitpivot$SIZE(global int *pivot) {
                *pivot = ~0;
            } 

            kernel void kbfindpivot$SIZE(global int *pivot, global $DTP *data, int rownum, int ipr) {
                int gid = get_global_id(0);
                global $DTP *row = data + rownum*ipr;
                if(gid<ipr && row[ipr] != 0) {
                    *pivot = gid;
                }
            }
            kernel void kbprint$SIZE(global int *pivot, 
                                     global $DTP *data,
                                     int nrows, int ncols, int ipr,
                                     global $DTP *unitmat,
                                     int urows, int ucols, int upr,
                                     int rownum) {
                printf("row %d: pivot=%d\n",rownum,*pivot);
                kbprintmat$SIZE(data,nrows,ncols,ipr);
                kbprintmat$SIZE(unitmat,urows,ucols,upr);
            }

            kernel void kbxorrow$SIZE(global int *pivot, 
                                      global $DTP *pivlist,
                                      global $DTP *kbflag,
                                      global $DTP *data,
                                      int nrows, int ncols, int ipr,
                                      global $DTP *unitmat,
                                      int urows, int ucols, int upr,
                                      int baserow) {
                int row = get_global_id(1), col = get_global_id(0);
                if(~0 == *pivot) {
                    // row is in kernel
                    if(0==col && row == baserow+1) printf("row %d in kernel\n",baserow);
                    return;
                }
                $DTP pval = data[row*ipr+*pivot];
                if(0==col) {
                    pivlist[row] = pval;
                } 
                $DTP pvalbase = data[baserow*ipr+*pivot];
                pvalbase ^= pvalbase & (pvalbase-1);
                if(0 != (pvalbase & pval)) {
                    unitmat[row*upr+col] ^= unitmat[baserow*upr+col];
                }
            }

            kernel void kbworrox$SIZE(global int *pivot, 
                                      global $DTP *pivlist,
                                      global $DTP *data,
                                      int nrows, int ncols, int ipr,
                                      int baserow) {
                int row = get_global_id(1), col = get_global_id(0);
                if(~0 == *pivot) return;
                $DTP pval = pivlist[row];
                $DTP pvalbase = data[baserow*ipr+*pivot];
                pvalbase ^= pvalbase & (pvalbase-1);
                if(0 != (pvalbase & pval)) {
                    data[row*ipr+col] ^= data[baserow*ipr+col];
                }
            }


            kernel void kernbasis$SIZE(global $DTP *data, // input matrix, will be destroyed in the process
                                       const int nrows, 
                                       const int ncols, 
                                       int ipr,    // number of 32-bit integers used for each row of "data"
                                       local int *pivot,
                                       local  $DTP *column,   // space to cache one $DTP-column of the input matrix
                                       global $DTP *unit,     // starts as unit matrix, will hold kernel and basis
                                       int uipr,   // number of 32-bit integers used for each row of "unit" 
                                       local  $DTP *rowcache, // space to cache one row of "unit" or "data"
                                       global $DTP *kbflag    /* bitmask whether row in "unit" belongs to kernel or basis */) {
                
                ipr  =  (ipr*32)/$SIZE;
                uipr = (uipr*32)/$SIZE;

                const $DTP maskatlastblock = (ncols % $SIZE) ? (1 << (ncols % $SIZE)) -1 : ~0;
                const int numblocks = (ncols+$SIZE-1) / $SIZE;

                int kerdim=0;

                if(get_global_id(0)==0) {
                    printf("size=%d, ncols=%d, numblocks=%d, maskatlastblock=%FMT\n",$SIZE,ncols,numblocks,maskatlastblock);
                    printf("kernbasis$SIZE: gsz=%d, lsz =%d, nrows=%d, ncols=%d, ipr=%d, uipr=%d, kbflag=%p\n",get_global_size(0),get_local_size(0),nrows,ncols,ipr,uipr,kbflag);
                }

                global $DTP *kbptr = kbflag;
                $DTP kbval = 0, kbbit = 1;
                for(int rownum=0;rownum<nrows;rownum++) {

                    // copy row to local cache and look for pivot
                    if(get_global_id(0)==0) *pivot = ~0;
                    barrier(CLK_LOCAL_MEM_FENCE); // sync for *pivot = ~0
                    {
                        global $DTP *rowptr = data+rownum*ipr;
                        int off=get_local_id(0);
                        for(rowptr += get_local_id(0);off<numblocks;off += get_local_size(0),rowptr += get_local_size(0)) {
                            $DTP val = *rowptr;
                            if(off == (numblocks-1)) {
                                val &= maskatlastblock;
                            }
                            rowcache[off] = val;
                            if(val) *pivot = off; 
                        }
                    }

                    barrier(CLK_LOCAL_MEM_FENCE); // use *pivot
                    if(0) {
                        if(get_global_id(0)==0) {
                            printf("rownum=%d\n",rownum);
                            kbprintmat$SIZE(data,nrows,ncols,ipr);
                        }
                        barrier(CLK_LOCAL_MEM_FENCE); // print matrix
                    }

                    if(*pivot != ~0) {
                        int poff = *pivot;
                        $DTP pval = rowcache[poff];
                        pval ^= pval & (pval-1);

                        if(get_global_id(0)==0) {
                            //printf("pivot for row %d in offset %d, bit %FMT\n",rownum,poff,pval);
                        }

                        for(int rownum2=rownum;++rownum2<nrows;) {
                            global $DTP *rowptr2 = data+rownum2*ipr;
                            #define needaction *pivot
                            if(get_global_id(0)==0) needaction = 0 != (rowptr2[poff] & pval);
                            //if(get_global_id(0)==0) printf("row %3d need action=%d, row = (%p,...(\n",rownum2,needaction,rowptr2);
                            barrier(CLK_LOCAL_MEM_FENCE); // sync for needaction
                            if(needaction) {
                                int off=get_local_id(0);
                                for(rowptr2 += get_local_id(0);off<numblocks;off += get_local_size(0),rowptr2 += get_local_size(0)) {
                                    *rowptr2 ^= rowcache[off];
                                }
                            }
                        }

                    } else {
                        if(get_global_id(0)==0) {
                            //printf("row %d in kernel\n",rownum);
                        }

                        kerdim++;
                        // row in kernel
                        if(get_global_id(0)==0) kbval |= kbbit;
                    }

                    if(0 == (kbbit <<= 1)) {
                        if(get_global_id(0)==0) *kbptr++ = kbval;
                        kbbit = 1;
                        kbval = 0;
                    }
                }

                if(get_global_id(0)==0) *kbptr++ = kbval;

                if(get_global_id(0)==0) {
                    int kdim=0;
                    kbptr = kbflag;
                    printf("kb=");
                    for(int j=0;j<nrows;j+=$SIZE) {
                        $DTP d = *kbptr; kdim += BITCOUNT(d);
                        printf("%FMT",*kbptr++);
                    }
                    $DTP d = *kbptr; kdim += BITCOUNT(d);
                    printf("%FMT / kdim = %d / kerdim = %d\n",*kbptr,kdim,kerdim);
                    printf("using %d bytes for kbptr\n",kbptr-kbflag);
                }

            }
        }]
    }

    append code {
        kernel void copyintbuffer(global const int *inbuf, global int *outbuf, int buflen) {
            int offset = get_global_id(0);
            if(offset < buflen) {
                outbuf[offset] = inbuf[offset];
            }
        }
    }

    # Steenrod algebra sequence numbers

    append code {

        int seqno_with_deg2(global const clenum *en, global const int *seqtab, global const exmo *ex, int deg) {
            int res=0, k, startk;
            const int tablen = en->tablen;
            startk = NALG-1; // MIN(en->pi->maxpowerXintI,NALG-1);
            // FIXME: try out loop unrolling
            for (k=startk; k--;) {
                global const int *seqtabk = seqtab + tablen*k;
                int prd, maxdeg, actdeg, exo;
                int rdgk = (2<<k)-1;
                maxdeg = en->maxdeg[k];
                exo = ex->r.dat[k];
                exo /= en->profile[k]; 
                exo *= en->profile[k];
                actdeg = exo * rdgk;
                if (deg < maxdeg) maxdeg = deg;
                if ((deg < actdeg)
                    || ((deg - actdeg) >= en->tablen)
                    || ((deg - maxdeg) >= en->tablen)) {
                        //printf("sizeof(exmo)=%d maxdeg=%d exmo.rdat[%d]=%d, deg=%d, actdeg=%d, tablen=%d\n",sizeof(exmo),maxdeg,k,ex->r.dat[k],deg,actdeg,en->tablen);
                    return -1;
                }
                //if((deg-actdeg)>=tablen || (deg-maxdeg)>=tablen) return -5;
                res += seqtabk[deg - actdeg] - seqtabk[deg - maxdeg];
                deg -= actdeg;
            }
            return res;
        }

        kernel void seqno_with_deg_ivect(global const clenum *en, global const int *seqtab,
                                         global const exmo *poly, int deg, global int *outbuf, int buflen) {
            int offset = get_global_id(0);
            if(offset < buflen) {
               outbuf[offset] = seqno_with_deg2(en,seqtab,poly+offset,deg);
            }
        }
    }

    # Steenrod algebra multiplication

    proc Loop {startup body breakcond} {
        lappend map STARTUP $startup BODY $body BREAKCOND $breakcond
        return [string map $map {
            STARTUP
            do {
                BODY
                BREAKCOND
            } while (1);
        }]
    }

    proc FirstRow {idx {labelprefix {}}} {
        set N [steenrod::prime 2 NALG]
        set code {}
        if {$labelprefix eq ""} {
            lappend code "/* fill row $idx */"
            lappend code "rem=ff<:$idx:>;"
        }
        for {set j 0} {$j<$N-1-$idx} {incr j} {
            if {$labelprefix ne ""} {
                if {!$j} continue
                lappend code "$labelprefix${idx}x$j: /* LABEL */"
            }
            set jc [expr {$N-1-$idx-$j}]
            set v x$jc$idx
            set d [format [expr {$jc>1 ? "d%d%d" : "x%d%d"}] [expr {$jc-1}] [expr {$idx+1}]]
            set dnext d$jc$idx
            set s s$jc$idx
            set sprev [format [expr {$jc+$idx+1<$N ? "s%d%d" : "0/*%d%d*/"}] $jc [expr {$idx+1}]]
            lappend code "$v = rem>>$jc;"
            lappend code "while(0 != ($v&$d)) $v--;"
            lappend code "rem -= $v << $jc;"
            lappend code "$dnext = $d | $v;"
            lappend code "$s = $sprev + $v;"
        }
        if {$labelprefix ne ""} {
            lappend code "$labelprefix${idx}x[expr {$j}]: /* LABEL */"
        }
        lappend code "x0$idx = rem;"
        join $code \n
    }

    proc NextRow {idx} {
        set N [steenrod::prime 2 NALG]
        set code {}
        lappend code "/* next row $idx */"
        lappend code "rem = x0$idx;"
        for {set j 1} {$j<$N-$idx} {incr j} {
            set jc $j
            set x x$j$idx
            set d [format [expr {$jc>1 ? "d%d%d" : "x%d%d"}] [expr {$jc-1}] [expr {$idx+1}]]
            set dnext d$jc$idx
            set s s$jc$idx
            set sprev [format [expr {$jc+$idx+1<$N ? "s%d%d" : "0/*%d%d*/"}] $jc [expr {1+$idx}]]
            set dupd "$dnext = $d|$x;"
            set supd "$s = $sprev+$x;"
            lappend code [subst {
                if($x) {
                    do {rem +=1<<$j;} while(0!=((--$x)&$d));
                    $dupd;
                    $supd;
                    goto nr${idx}x[expr {$N-$idx-$j}];
                }
            }]
        }
        lappend code "break; /* row $idx is finished */"
        join $code \n
        return "[join $code \n]\n[FirstRow $idx nr]"
    }

    proc MultCode {} {
        set N [steenrod::prime 2 NALG]
        set code {
            /* main */
            /* ADD CALLBACK HERE */
            /* use a barrier to enforce synchronicity between the threads in this workgroup.
            ** this barrier only exists for performance reasons, based on the belief that
            ** synchronous enumeration of the admissible matrices might be advantageous. */ 
            barrier(CLK_LOCAL_MEM_FENCE);
        }
        for {set i 0} {$i<$N-1} {incr i} {
            set code [Loop [FirstRow $i] $code [NextRow $i]]
        }

        set vars ""
        for {set i 0} {$i<$N} {incr i} {
            set sep "uchar "
            for {set j 0} {$j<$N-$i} {incr j} {
                append vars $sep.$j$i
                set sep ", "
            }
            append vars ";\n"
        }
        lappend vardefs [string map {. x} $vars]
        lappend vardefs [string map {. s} $vars]
        lappend vardefs [string map {. d} $vars]
        set vardefs [join $vardefs \n]

        string map [list MULTCODE $code VARDECL $vardefs] {
            __kernel void steenrod_multiplication(
            __global const uchar *ffdata, // first factor is shared for whole work group
            __global const uchar *sfdata, // array of second factors, distributed over work group
            __global int *result)
            {
                uchar rem;
                VARDECL
                #define isdbg ((get_local_id(0)==0) && (get_local_id(1)==0))
                __global const uchar *ff = ffdata + 8*get_global_id(0);
                __global const uchar *sf = sfdata + 8*get_global_id(1);
                MULTCODE
            }
        }
    } 

    proc AutoIndent {code} {
        set lines {}
        set ind 5
        foreach x [split $code \n] {
            set x [string trim $x]
            if {$x eq ""} continue
            set x [string map {<: \[ :> \]} $x]
            if {[string first "\}" $x]>=0 || [string first LABEL $x]>=0} {
                incr ind -1
            }
            lappend lines [string repeat "  " $ind]$x
            if {[string first "\{" $x]>=0 || [string first LABEL $x]>=0} {
                incr ind +1
            }
        }
        join $lines \n
    }

    append code {
        void print_steenrod(int issf, __global const uchar *a) {
            if(issf) {
                printf("sf=Sq(%d,%d,%d,%d,%d,%d,%d,%d)",a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]);
            } else {
                printf("ff=Sq(%d,%d,%d,%d,%d,%d,%d,%d)",a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]);
            }
        }
    }

    append code [AutoIndent [MultCode]]

    #puts code=$code

    namespace eval kernels {}

    proc build-kernels {} {
        variable code
        set options "-DHOST_INT=$::steenrod::cl::intType -DHOST_INTSIZE=$::steenrod::cl::intSize -Werror"
        Log debug "Building program with $options"
        ctx program thecode $code $options
        lappend map CL_KERNEL "\n   CL_KERNEL"
        foreach ker [lsort [thecode list]] {
            thecode kernel ::steenrod::cl::kernels::$ker $ker
            Log debug "Kernel '$ker': [string map $map [kernels::$ker workgroupinfo]]"
        }
    }

    namespace eval mat2 {
        namespace eval buf {}

        namespace path {
            ::steenrod::cl::kernels
            ::steenrod::cl
            ::steenrod
        }

        # create a pseudo random matrix with given dimensions
        proc random {nrows ncols {rounds 27} {seed 4711}} {
            matrix clcreate buf::ans $nrows $ncols dims {
                randomize-buffer buf::ans $dims $rounds $seed
            }
        }

        proc randomize-buffer {buf dims rounds seed} {
            foreach {nrows ncols ipr} $dims break
            random_matrix setarg 0 buffer $buf
            random_matrix setarg 1 int $ipr
            random_matrix setarg 2 int $seed
            random_matrix setarg 3 int $rounds
            ctx enqndr 1 random_matrix {0 0} [list $nrows $ipr] {1 1} {} evt "randomize buffer $dims, $rounds rounds, seed $seed"
            cl event wait evt
        }

        # create a pseudo random matrix with given dimensions and (approximately) 
        # prescribed rank. the matrix is created as M_ij = <a_i,b_j> with random
        # vectors a,b from a space of dimension $rank
        proc rank-random {nrows ncols rank {rounds 27} {seed 76121}} {
            matrix clcreate buf::ans $nrows $ncols dims {
                rank-random-matrix buf::ans $dims $rank $rounds $seed
                buf::ans dispose
            }
        }

        proc rank-random-matrix {buf dims rank rounds seed} {
            foreach {nrows ncols ipr} $dims break
            rank_random_matrix setarg 0 buffer $buf
            rank_random_matrix setarg 1 integer $nrows
            rank_random_matrix setarg 2 integer $ncols
            rank_random_matrix setarg 3 integer $ipr
            rank_random_matrix setarg 4 integer $rank
            rank_random_matrix setarg 5 integer $seed
            rank_random_matrix setarg 6 integer $rounds
            array set kinfo [rank_random_matrix workgroupinfo]
            set min $kinfo(CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE)
            set wsz [expr {$min*(($rank+$min-1)/$min)}]
            if {$wsz > [set max [cl config CL_DEVICE_MAX_WORK_GROUP_SIZE]]} {
                set wsz $max
            }
            set nrows2 [expr {$wsz*(($nrows+$wsz-1)/$wsz)}]
            set bufsize [expr {(($rank+31)/32)*32*4}]
            rank_random_matrix setarg 7 local $bufsize
            ctx enqndr 1 rank_random_matrix {0 0} [list $nrows2 $ipr] [list $wsz 1] {} evt "rank-random-matrix $dims $rank $rounds $seed"
            cl event wait evt
        }

        proc kerbas-requirements {nrows ncols bitsize -> colcachesz rowcachesz kbflagssz} {
            upvar 1 $colcachesz cc $rowcachesz rc $kbflagssz kb
            set b $bitsize
            set kb 0
            set cc 4;#[expr {($b*$nrows)/8}]
            set rc1 [expr {($b*$nrows)/8}]
            set rc2 [expr {($b*$ncols)/8}]
            set rc [expr {max($rc1,$rc2)}]
        }


        proc kerbas {mvar kvar bvar {bitsizes {64 32 16 8}}} {
            upvar 1 $mvar m $kvar k $bvar b
            # compute kernel and basis of the matrix in mvar
            foreach {nrows ncols} [matrix dimensions $m] break

            set b 1

            buffer alloc buf::pivot 4 
            set size 32
            set ip kbinitpivot$size
            set fp kbfindpivot$size
            set xr kbxorrow$size
            set rx kbworrox$size
            set dbg kbprint$size 
            set numintscol [expr {($size-1+$ncols)/$size}]
            set numintsrow [expr {($size-1+$nrows)/$size}]
            buffer alloc buf::kbflags [expr {$numintsrow*$size/8}] 
            buffer alloc buf::pivlist [expr {$nrows*$size/8}] 
            $ip setarg 0 buffer buf::pivot
            $fp setarg 0 buffer buf::pivot
            $xr setarg 0 buffer buf::pivot
            $xr setarg 1 buffer buf::pivlist
            $xr setarg 2 buffer buf::kbflags

            $rx setarg 0 buffer buf::pivot
            $rx setarg 1 buffer buf::pivlist

            set k [matrix clcreate buf::unit $nrows $nrows udims {
                unitmatrix setarg 0 buffer buf::unit
                RunKernel unitmatrix $udims unitevt

                $xr setarg 7 buffer buf::unit
                $xr setarg 8 integer [lindex $udims 0]
                $xr setarg 9 integer [lindex $udims 1]
                $xr setarg 10 integer [lindex $udims 2]
                    
                matrix clmap buf::mat CL_MEM_USE_HOST_PTR m dims {
                    $xr setarg 3 buffer buf::mat
                    $xr setarg 4 integer [lindex $dims 0]
                    $xr setarg 5 integer [lindex $dims 1]
                    $xr setarg 6 integer [lindex $dims 2]

                    $rx setarg 2 buffer buf::mat
                    $rx setarg 3 integer [lindex $dims 0]
                    $rx setarg 4 integer [lindex $dims 1]
                    $rx setarg 5 integer [lindex $dims 2]

                    $dbg setarg 0 buffer buf::pivot
                    $dbg setarg 1 buffer buf::mat
                    $dbg setarg 2 integer [lindex $dims 0]
                    $dbg setarg 3 integer [lindex $dims 1]
                    $dbg setarg 4 integer [lindex $dims 2]
                    $dbg setarg 5 buffer buf::unit
                    $dbg setarg 6 integer [lindex $udims 0]
                    $dbg setarg 7 integer [lindex $udims 1]
                    $dbg setarg 8 integer [lindex $udims 2]

                    $fp setarg 1 buffer buf::mat
                    $fp setarg 3 integer [lindex $dims end]
                    set event {}
                    for {set row 0} {$row<$nrows} {incr row} {
                        $fp setarg 2 integer $row
                        $xr setarg 11 integer $row
                        $rx setarg 6 integer $row
                        ctx enqtask 1 $ip $event evip "reset pivot"
                        ctx enqndr  1 $fp {} $numintscol {} evip evfp "find pivot row $row"
                        $dbg setarg 9 integer $row
                        #ctx enqtask 1 $dbg evfp evfp2 "print pivot"
                        set x [expr {$nrows-$row-1}]
                        set y [expr {$row+1}]
                        set event evfp
                        if {$x} {
                            ctx enqndr  1 $xr [list 0 $y] [list $numintsrow $x] {} evfp evxr "operate unit matrix $row"
                            ctx enqndr  1 $rx [list 0 $y] [list $numintscol $x] {} evxr evrx "operate input matrix $row"
                        }
                        set event evrx
                    }
                    buf::mat dispose
                }  
                cl event wait $event
                buf::unit dispose
            }]
        }

        proc kerbas2 {mvar kvar bvar {bitsizes {64 32 16 8}}} {
            upvar 1 $mvar m $kvar k $bvar b
            # compute kernel and basis of the matrix in mvar
            foreach {nrows ncols} [matrix dimensions $m] break

            # our kernel runs in a single workgroup. it exists in different bit sizes b=8,16,32,64
            # it requires the following local buffers
            #    column  = a block column of width b in mvar (size nrows*(b/8) bytes)
            #    unitrow = a row in an associated nrow*nrow matrix (size nrows/8 bytes, apropriately rounded)
            set lsz [config CL_DEVICE_LOCAL_MEM_SIZE]
            set bitsize -1
            foreach b $bitsizes {
                kerbas-requirements $nrows $ncols $b -> colcachesz rowcachesz kbflagssz
                if {$colcachesz + $rowcachesz + $kbflagssz <= $lsz} {
                    set bitsize $b
                    break
                } 
            }
            if {$bitsize<0} {
                error "not enough local memory for kerbas ${nrows}x${ncols}: need $colcachesz + $rowcachesz + $kbflagssz, have $lsz"
            }            
            set maxsz [config CL_DEVICE_MAX_WORK_GROUP_SIZE]
            #set maxsz 128 ;# for testing

            buffer alloc buf::kbflags [set kbsize [expr {(($bitsize-1+$nrows)/$bitsize)*$bitsize}]]
            puts "allocated $kbsize bytes for kbflag"

            set kernel kernbasis$bitsize
            set k [matrix clcreate buf::unit $nrows $nrows udims {
                unitmatrix setarg 0 buffer buf::unit
                RunKernel unitmatrix $udims unitevt
                matrix clmap buf::mat CL_MEM_USE_HOST_PTR m dims {
                    $kernel setarg 0 buffer buf::mat 
                    $kernel setarg 1 integer $nrows
                    $kernel setarg 2 integer $ncols
                    $kernel setarg 3 integer [lindex $dims end]
                    $kernel setarg 4 local   4
                    $kernel setarg 5 local   $colcachesz
                    $kernel setarg 6 buffer buf::unit
                    $kernel setarg 7 integer [lindex $udims end]
                    $kernel setarg 8 local   $rowcachesz
                    $kernel setarg 9 buffer buf::kbflags
                    puts [$kernel workgroupinfo]
                    ctx enqndr 1 $kernel 0 $maxsz $maxsz unitevt evt "$kernel $dims"
                    buf::mat dispose
                }  
                cl event wait evt
                buf::unit dispose
            }]
        }

        proc RunKernel {kernel dims evtvar} {
            upvar 1 $evtvar evt
            foreach {nrows ncols ipr} $dims break
            $kernel setarg 1 integer $ipr
            $kernel setarg 2 integer $nrows
            $kernel setarg 3 integer $ncols
            set isz [expr {8*$::steenrod::cl::intSize}]
            set colpr [expr {($ncols+$isz-1)/$isz}]
            ctx enqndr 1 $kernel {0 0} [list $nrows $colpr] [list 1 $colpr] {} evt "$kernel $dims"
        }

        proc zero {nrows ncols} {
            matrix clcreate buf::ans $nrows $ncols dims {
                zeromatrix setarg 0 buffer buf::ans
                RunKernel zeromatrix $dims evt
                cl event wait evt
                buf::ans dispose
            }
        }
        proc unit {nrows {ncols -1}} {
            if {$ncols<0} {set ncols $nrows}
            matrix clcreate buf::ans $nrows $ncols dims {
                unitmatrix setarg 0 buffer buf::ans
                RunKernel unitmatrix $dims evt
                cl event wait evt
                buf::ans dispose
            }
        }

        namespace export kerbas random rank-random zero unit
        namespace ensemble create
    }


    namespace eval comp2 {
        namespace eval buf {}

        namespace path {
            ::steenrod::cl::kernels
            ::steenrod::cl
            ::steenrod
        }

        proc image {} {

        }

        proc matrix {enumsrc mmp enmdst {progvarname {}} {progsteps 10}} {
            set maxsz [config CL_DEVICE_MAX_WORK_GROUP_SIZE]
            set srcdim [$enumsrc dimension]
            set dstdim [$enumdst dimension]
            $enumdst clmap buf::seqno sparams
            matrix clcreate buf::mat $srcdim $dstdim mdims {
                zeromatrix setarg 0 buffer buf::mat
                ::steenrod::cl::mat2::RunKernel zeromatrix $mdims evt
                cl event wait evt
                compmat setarg 0 buffer buf::mat
                compmat setarg 1 integer [lindex $mdims 2]
                compmat setarg 2 buffer buf::seqno
                Foreach-SrcPoly buf::p buf::dg pcnt dgcnt rowcnt $enumsrc $mmp {
                    compmat setarg 2 buffer buf::p
                    compmat setarg 3 integer $pcnt
                    compmat setarg 4 buffer buf::dg
                    compmat setarg 5 integer $dgcnt
                    compmat setarg 6 integer $rowcnt
                    compmat setarg 4 integer $pcnt
                    ctx enqndr 1 compmat {0 0} [list $pcnt $dgcnt] [list 1 $maxsz] {} evt "compmat ($srcdim x $dstdim: $rowcnt...)"
                }
            }
            buf::seqno dispose
        }

        proc ComputeMatrix {enmsrc mmp enmdst -> clmat {eventlist {}} {waitvar {}} {desc {}}}  {
            upvar 1 $clmat mat $waitvar wv
            if {$desc eq ""} {
                set desc "ComputeMatrix"
            }
            set nrows [$enmsrc dimension]
            set ncols [$enmdst dimension]
            ::steenrod::matrix clalloc buf $N $M dimsvar
            ::steenrod::cl::mat2::RunKernel zeromatrix $mdims evt
            lappend eventlist $evt
            ...
            ::steenrod::matrix clload buf $dimsvar {} wv "$desc: fetch result"
        }

        proc ComputeMatrixAll {enmsrc mmp enmdst matvar bdy} {
            upvar 1 $matvar m
            $enmsrc sigreset
            $enmdst sigreset
            set cnt 0
            while on {

                set m [ComputeMatrix $enmsrc $mmp $enmdst]
                uplevel 1 $bdy
                if {![$enmdst signext] || ![$enmsrc signext]} break
                incr cnt
            }
        }

        namespace export image matrix ComputeMatrixAll
        namespace ensemble create
    }

    proc run-selftest {} {

    }

    proc init {} {
        choose-device
        build-kernels
        run-selftest
    }

    proc config {flag} {
        variable cfgcache
        if {![info exists cfgcache]} {
            array set cfgcache [ctx platform]
            array set cfgcache [ctx device]
        }
        switch -- $flag {
            names { return [array names cfgcache] }
            get   { return [array get cfgcache] }
        }
        return $cfgcache($flag)
    }

    namespace eval event {
        proc info {varname} {
            upvar 1 $varname v
            ::steenrod::cl::ctx eventinfo v
        }
        namespace export info move history wait
        namespace ensemble create
    }

    namespace eval buffer {
        proc intlist {bufobj} {
            set sz [$bufobj size]
            value $bufobj val
            binary scan $val i* out
            return $out
        }
        namespace export value create allocate intlist
        namespace ensemble create
    }

    namespace export init ctx dump config event buffer mat2
    namespace ensemble create
}


