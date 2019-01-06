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

    set code {
        kernel void unitmatrix(__global HOST_INT *h, int wtf) {
            //
        }
    }

    append code {
        // code to create random matrices with limited ranks, used for testing
        // based on http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/

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

        kernel void random_matrix(global HOST_INT *data, int ipr, int seed, int rounds) {
            uint row = get_global_id(0), col = get_global_id(1);
            uint offset = row*ipr + col;
            uint rdata = wang_hash(offset+seed);
            while(rounds--) {
                rdata ^= (rdata << 13);
                rdata ^= (rdata >> 17);
                rdata ^= (rdata << 5);
            }
            data[offset] = rdata;
        }

        kernel void scalar_product(global HOST_INT *data, 
                                   uint nrows, uint ncols, uint ipr,
                                   global HOST_INT *a, uint aipr,
                                   global HOST_INT *b, uint bipr,
                                   uint rank,
                                   local HOST_INT *c) {
            uint row = get_global_id(0), col = get_global_id(1);
            uint offset = row*ipr + col;

#define DBGROW 0
#define DBGCOL 1
#define IFDBG if(row==DBGROW && col==DBGCOL)

            IFDBG {
                printf("row=%d,col=%d,lsz(0)=%d, lsz(1)=%d\n",row,col,get_local_size(0),get_local_size(1));
                printf("nrows=%d,ncols=%d,ipr=%d,aipr=%d,bipr=%d,rank=%d\n",nrows,ncols,ipr,aipr,bipr,rank);
                printf("data=%p,a=%p,b=%p,c=%p\n",data,a,b,c);
            }

            {
                // begin copying N rows from b into the local memory at c
                // where N is the number of bits in a HOST_INT
                const uint N = sizeof(HOST_INT)*8;
                uint col2 = N*col, col3 = min((uint) N*(col+1), (uint) ncols);
                IFDBG {printf("N=%d,col2=%d,col3=%d\n",N,col2,col3);}
                global HOST_INT *bstart = b + bipr*col2 + get_local_id(0);
                global HOST_INT *bend   = b + bipr*col3;
                local HOST_INT *cstart = c + get_local_id(0);
                IFDBG {printf("b=%p, c=%p, bend=%p\n",bstart,cstart,bend);}
                while(bstart < bend) {
                    IFDBG {printf("b=%p, c=%p, bend=%p, *bstart=%d\n",bstart,cstart,bend,*bstart);}
                    *cstart = *bstart;
                    cstart += get_local_size(0);
                    bstart += get_local_size(0);
                }
                barrier(CLK_LOCAL_MEM_FENCE);
            }

            if(row>=nrows) return;

            global HOST_INT *arow = a+aipr*row;
            uint ans = 0;
            for(uint bit=1;bit;bit<<=1,c+=aipr) {
                int bc = 0;
                for(int i=0;i<aipr;i++) {
                    HOST_INT x = c[i] & arow[i]; 
                    bc += BITCOUNT(x);
                    IFDBG{printf("bit=%d,i=%d,c=%p,c[i]=%d,arow[i]=%d,x=%d,bc=%d\n",bit,i,c,c[i],arow[i],x,bc);}
                }
                if(0!=(bc&1)) ans ^= bit;
            }
            data[offset] = ans;
        }

    }


    foreach size {8 16 32 64} dtp {char short int long} {
        set map [list "\$SIZE" $size "\$DTP" $dtp]
        append code [string map $map {
            kernel void kernbasis$SIZE(global HOST_INT* data,
                                       int nrows, int ncols, int ipr,
                                       local $DTP* column,
                                       global HOST_INT* unit, int uipr,
                                       local $DTP* unitrow,
                                       global $DTP *kbflag) {


            }
        }]
    }
    puts code=$code

    namespace eval kernels {}

    proc build-kernels {} {
        variable code
        set options "-DHOST_INT=$::steenrod::cl::intType -DHOST_INTSIZE=$::steenrod::cl::intSize"
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
            random_matrix setarg 1 int [lindex $dims 2]
            random_matrix setarg 2 int $seed
            random_matrix setarg 3 int $rounds
            ctx enqndr 1 random_matrix {0 0} [list $nrows $ipr] {1 1} {} evt "randomize buffer $dims, $rounds rounds, seed $seed"
            cl event wait evt
        }

        # create a pseudo random matrix with given dimensions and rank
        # the matrix is created as M_ij = <a_i,b_j> with random vectors
        # a,b from a space of dimension $rank
        proc rank-random {nrows ncols rank {rounds 27} {seed 76121}} {
            matrix clcreate buf::ans $nrows $nrows dims {
                matrix clalloc buf::a $nrows $rank adims {
                    randomize-buffer buf::a $adims $rounds $seed
                    matrix clalloc buf::b $ncols $rank bdims {
                        randomize-buffer buf::b $bdims $rounds $seed
                        scalar-product-matrix buf::ans buf::a buf::b $dims $adims $bdims
                    }
                }
                buf::a dispose
                buf::b dispose
                buf::ans dispose
            }
        }

        proc scalar-product-matrix {buf a b dims adims bdims} {
            foreach {nrows ncols ipr} $dims break
            foreach {arows acols apr} $adims break
            foreach {brows bcols bpr} $bdims break
            scalar_product setarg 0 buffer $buf
            scalar_product setarg 1 integer $nrows
            scalar_product setarg 2 integer $ncols
            scalar_product setarg 3 integer $ipr
            scalar_product setarg 4 buffer $a
            scalar_product setarg 5 integer $apr
            scalar_product setarg 6 buffer $b
            scalar_product setarg 7 integer $bpr
            scalar_product setarg 8 integer $acols
            # allocate local memory for intSize*8 rows of b
            set lsz [expr {$bpr*8*($::steenrod::cl::intSize)**2}]
            puts "using lsz=$lsz"
            scalar_product setarg 9 local $lsz
            array set kinfo [scalar_product workgroupinfo]
            set wsz $kinfo(CL_KERNEL_WORK_GROUP_SIZE)
            set nrows2 [expr {($nrows/$wsz)*$wsz}]
            if {$nrows2<$nrows} {incr nrows2 $wsz}
            ctx enqndr 1 scalar_product {0 0} [list $nrows2 $ipr] [list $wsz 1] {} evt "scalar product $dims"
            cl event wait evt
        }

        proc kerbas {mvar kvar bvar} {
            # compute kernel and basis of the matrix in mvar
            foreach {nrows ncols} [matrix dimensions $mvar] break

            # our kernel runs in a single workgroup. it exists in different bit sizes b=8,16,32,64
            # it requires the following local buffers
            #    column  = a block column of width b in mvar (size nrows*(b/8) bytes)
            #    unitrow = a row in an associated nrow*nrow matrix (size nrows/8 bytes, apropriately rounded)
            set lsz [config CL_DEVICE_LOCAL_MEM_SIZE]
            set bitsize -1
            foreach b {64 32 16 8} {
                set sz1 [expr {($nrows*$b/8)}]
                set sz2 [expr {(($nrows+$b-1)/$b)*($b/8)}]
                if {$sz1+$sz2<=$lsz} {
                    set bitsize $b
                    break
                } 
            }
            if {$bitsize<0} {
                error "not enough local memory for kerbas ${nrows}x${ncols}: need $sz1+$sz2, have $lsz"
            }
            set kb [matrix clcreate unit $nrows $nrows dims {
                unitmatrix setarg 0 buffer unit
                unitmatrix setarg 1 integer $nrows
            }]
        }

        namespace export kerbas random rank-random
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
        namespace export value create
        namespace ensemble create
    }

    namespace export init ctx dump config event buffer mat2
    namespace ensemble create
}


