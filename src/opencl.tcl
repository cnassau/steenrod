package provide Steenrod::opencl 1.0

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
        __kernel void unitmatrix(__global HOST_INT *h, int wtf) {
            //
        }
    }

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
        namespace path {
            ::steenrod::cl::kernels
            ::steenrod::cl
            ::steenrod
        }

        proc kerbas {mvar kvar bvar} {
            # compute kernel and basis of the matrix in mvar
            foreach {nrows ncols} [matrix dimensions $mvar] break
            set kb [matrix clcreate unit $nrows $nrows dims {
                unitmatrix setarg 0 buffer unit
                unitmatrix setarg 1 integer $nrows
            }]
        }

        namespace export kerbas
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

        namespace export info move
        namespace ensemble create
    }

    namespace eval buffer {
        namespace export value create
        namespace ensemble create
    }

    namespace export init ctx dump config event buffer
    namespace ensemble create
}


