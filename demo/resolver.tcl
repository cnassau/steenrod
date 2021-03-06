#!/usr/bin/tclsh
#
# Resolve an algebra and display the results in a canvas
#
# Copyright (C) 2004-2019 Christian Nassau <nassau@nullhomotopie.de>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
 
lappend auto_path ../lib

if {[catch {package require Steenrod} err]} {
    puts "Could not load Steenrod algebra library."
    puts "(Error message: $err)"
    puts "Check your installation and/or adjust the auto_path settings in [info script]"
    exit 1
}

namespace import -force steenrod::*

array set defoptions {
    -prime   { {} "the prime" }
    -algebra { {} "the algebra" }
    -usegui  { 1  "use graphical user interface ?" }
    -maxdim  { 40 "maximal topological dimension" }
    -maxs    { 50 "maximal homological degree" }
    -dbg     { 0  "log computation, used for debugging" }
    -decomp  { auto "smartness: either auto, upper, lower, none" }
    -repcnt  { 1  "number of repetitions (debug parameter)" }
    -viewtype { odd "use 'even' for traditional charts at p=2"}
    -record  { off "whether to record decomposition details" }
}

foreach {opt val} [array get defoptions] {
    set options($opt) [lindex $val 0]
}

proc usage {} {
    global defoptions
    puts "usage: [info script] ?option value? ?option value? ..."
    puts ""
    puts "available options:"
    puts ""
    foreach {opt val} [array get defoptions] { lappend aux [list $opt $val] }
    foreach itm [lsort -index 0 $aux] {
        foreach {opt val} $itm break
        set msg [format "%10s : %s" $opt [lindex $val 1]]
        if {[lindex $val 0]!=""} { append msg " (default: [lindex $val 0])" }
        puts $msg
    }
    puts ""
    puts "-> TODO: explain algebra format <-"
    puts ""
    puts "Example: for A(2) use '-algebra \"0 0 {3 2 1} 0\"'"
    exit 1
}

foreach {opt val} $argv {
    if {![info exist options($opt)]} { usage }
    set options($opt) $val
}

if {$options(-prime)==""} { usage }

if {!$options(-record)} {
    proc rec args {}
} else {
    interp alias {} rec {} eval 
}

# see if options make sense
enumerator x 
if {[catch {
    x co -prime $options(-prime) -algebra $options(-algebra)
    x dim
} err]} {
    puts "cannot set -prime $options(-prime) -algebra $options(-algebra): $err"
    exit 1
}
rename x ""

set p   $options(-prime)
set alg $options(-algebra)

proc tryscan {opt var} {
    global options
    upvar $var x
    if {![scan $options($opt) %d x]} { 
        puts "'$opt' needs integer argument (from '$opt $options($opt)')" 
        exit 1
    }
}

tryscan -maxdim maxdim
tryscan -maxs maxs
tryscan -usegui usegui

tryscan -repcnt repcnt

# debugging aids: 

set dbgflag "to-be-overwritten"
set errinf {}

tryscan -dbg dbgflag

proc dbgclear {} { 
    global errinf
    set errinf {}
}

proc dbgprint {} {
    global errinf dbgflag
    if {!$dbgflag} { 
        puts "debug logging not enabled (rerun with '-dbg 1')"
    } else {
        puts [join $errinf \n]
    }
}

proc dbgadd {body} {
    global dbgflag
    if {$dbgflag} {
        uplevel 1 [concat lappend ::errinf $body] 
    }
}

incr maxs

# newgen is a callback function that can be overwritten in the GUI.
# The GUI can also put a trace on the tridegree and theprofile variables. 
# Finally, when we're through we'll call the finito proc.

set tridegree  {} 
set theprofile {}

set mathomdimfmt "%3d->%3d->%3d"
set matliftdimfmt "%3d->%3d"
set mathomdim ""
set matliftdim ""

proc finito {} { puts "Done." }

proc newgen {id s e i diff} {
    puts [format "%3d / %3d : gen %d (pro $::theprofile) (diff = %s)" $s $i $id $diff]
}

if {$usegui} {
    source [file join [file dirname [info script]] simplegui.tcl]
}

# Routines that determine the profile of the maximal allowable subalgebra

set useupper 1
set uselower 1

switch -glob -- $options(-decomp) {
    au* { foreach {useupper uselower} {1 1} break }
    lo* { foreach {useupper uselower} {0 1} break }
    up* { foreach {useupper uselower} {1 0} break }
    no* { foreach {useupper uselower} {0 0} break }
    default {
        puts "unknown -decomp value '$options(-decomp)'. Should be auto, upper, lower, none"
        exit 1
    }
}

proc maxUpperProfile {prime algebra s ideg edeg} {
    global useupper
    incr s 0

    if {!$useupper} { return {0 0 0 0} }

    set edegs [prime $prime edegrees]
    set rdegs [prime $prime rdegrees]
    set tpmo  [prime $prime tpmo]

    set NALG [llength $rdegs]

    set edat 0
    set rdat {}

    foreach dg $rdegs {
        set slope [expr $tpmo * $dg]
        if {[expr $slope * $s > $ideg]} { 
            lappend rdat $NALG
        } else {
            lappend rdat 0
        }
    }

    set bit 1
    foreach dg $edegs {
        set slope $dg
        if {[expr $slope * $s > $ideg]} { 
            set edat [expr $edat | $bit]
        } 
        set bit [expr $bit << 1]
    }

    return [list 0 $edat $rdat 0]
}

proc maxLowerProfile {prime algebra s ideg edeg} { 
    global uselower
    
    incr s 1

    if {!$uselower} { return {0 0 0 0} }

    set edegs [prime $prime edegrees]
    set rdegs [prime $prime rdegrees]
    set tpmo  [prime $prime tpmo]
    set NALG  [llength $rdegs]

    if {$algebra=={}} { 
        set aux {}
        foreach dg [prime $prime rdegrees] { lappend aux $NALG }
        set algebra [list 0 -1 $aux 0]
    }
    
    set ae [lindex $algebra 1]
    set ar [lindex $algebra 2]

    # ext and red describe the profile of the used subalgebra B
    set ext 0
    set red {}
    foreach dg [prime $prime rdegrees] { lappend red 0 }

    # dB is the maximal slope, tB the maximal dimension of B
    set dB 0
    set tB 0

    set bit 1
    for {set i 0} {1} {incr i} {

        if {[set slope [lindex $edegs $i]] > $ideg} {
            # cannot increase B for dimensional reasons, so return
            return [list 0 $ext $red 0]
        }
        
        if {$bit & $ae} {
            
            incr tB $slope
            if {$slope > $dB} { set dB $slope }

            if {[expr $ideg - $tB - $s * $dB] < 0} {
                # next profile too big, so return current one 
                return [list 0 $ext $red 0]
            }

            set ext [expr $ext | $bit]
        }
        
        for {set j $i} {[incr j -1] >= 0} {} {
            
            set arx [lindex $ar $j]
            set rdx [lindex $red $j]
            
            # if there's no more room in the $algebra, just continue
            if {$rdx >= $arx} continue

            set slope [expr ($prime - 1) * $tpmo * [lindex $rdegs $j]]
            if {$slope > $dB} { set dB $slope }
            incr tB $slope
            
            if {[expr $ideg - $tB - $s * $dB] < 0} {
                # next profile too big, so return current one 
                return [list 0 $ext $red 0]
            }

            lset red $j [expr $rdx + 1]
        }
        
        set bit [expr $bit << 1] 
    }

    error not-reached!
}

switch -regexp -nocase -- $options(-viewtype) {
    ^o {set ::viewtypeeven 0}
    ^e {set ::viewtypeeven 1}
    default {error "viewtype $options(-viewtype) not understood"}
}

while {$repcnt} { 

    # create empty modules and maps
    for {set i -2} {$i<=$maxs} {incr i} {
        enumerator C$i -prime $p -algebra $alg
        monomap d$i
    }

    # fake generator from Ext^0
    C0 configure -genlist [expr 0 * 0]

    set maxe $maxs

    # Main loop starts here:

    newgen 0 0 0 0 {} ;# fake gen of deg zero
    
    for {set sdeg 0} {$sdeg<$maxs} {incr sdeg} {

        # delete unused modules & maps to save memory
        if {$sdeg>=2} {
            rename C[expr $sdeg-2] ""
            rename d[expr $sdeg-2] ""
        }

        if {$::viewtypeeven} {
            set maxi [expr {2*($maxdim + $sdeg)}]
        } else {
            set maxi [expr {$maxdim + $sdeg}]
        }
        
        for {set ideg $sdeg} {$ideg<=$maxi} {incr ideg} {

            for {set edeg 0} {($edeg<=[expr $sdeg+1]) && ($edeg<=$maxe)} {incr edeg} {

                update

                # resolve tridegree (s,e,t) == ($sdeg, $edeg, $ideg)
                
                # determine previous, current, and next value of s
                set sp [expr $sdeg-1]
                set sc $sdeg
                set sn [expr $sdeg+1]
                
                set alldims {}
                foreach hdg [list $sp $sc $sn] {
                    C$hdg configure -edeg $edeg -ideg $ideg 
                    C$hdg configure -profile {} -sig {}
                    lappend alldims [C$hdg dimension]
                }

                # find maximal possible profiles  
                set pupper [maxUpperProfile $p $alg $sc $ideg $edeg]
                set plower [maxLowerProfile $p $alg $sc $ideg $edeg]

                # count dimensions
                C$sc configure -profile $pupper -sig {} ; set updim [C$sc dimension]
                C$sc configure -profile $plower -sig {} ; set lodim [C$sc dimension]

                if 0 {
                    puts "(s,i)=($sdeg,$ideg), t-s=[expr $ideg-$sdeg], edeg=$edeg"
                    puts "  lowprof = $plower, dim = $lodim"
                    puts "  uppprof = $pupper, dim = $updim"
                    C$sc configure -profile $pupper ; puts "C$sc conf = [C$sc conf]"
                    C$sc configure -profile $plower ; puts "C$sc conf = [C$sc conf]"
                }

                if {(0==$updim) || (0==$lodim)} continue

                if {$updim > $lodim} { 
                    set theprofile $plower
                } else {
                    set theprofile $pupper
                }
                
                # at this point the gui's trace will trigger
                set tridegree [list $sdeg $ideg $edeg]

                puts "(s:$sdeg, i:$ideg, e:$edeg) : profile $theprofile"

                foreach mod [list C$sp C$sc C$sn] { 
                    $mod configure -profile $theprofile 
                    $mod sigreset
                    # puts "$mod config = [$mod config]"
                    # puts "$mod basis = [$mod basis]"
                }

                # now compute matrices   mdsc : Cs -> Cs-1  and  mdsn : Cs+1 -> Cs
                if {0 == $sc} {
                    # fake appropriate matrix
                    if {($edeg==0) && ($ideg==0)} {
                        set mdsc 1
                    } else {
                        # here C-1 = F_p = 0. create a zero matrix with the
                        # right number of rows.
                        set mdsc [matrix create [C0 dim] 1]
                    }                
                    eval set mdsn \[steenrod::ComputeMatrix C$sn d$sn C$sc\]  
                } else {
                    # using eval here gives nicer error reports
                    eval set mdsc \[steenrod::ComputeMatrix C$sc d$sc C$sp\]  
                    eval set mdsn \[steenrod::ComputeMatrix C$sn d$sn C$sc\]  
                }

                foreach {nr nc} [matrix dimensions $mdsn] break
                foreach {cr cc} [matrix dimensions $mdsc] break
                set mathomdim [format $mathomdimfmt $nr $nc $cc]
                set matliftdim ""

                # compute image...
                matrix ortho $p mdsn ker  ;# (ker is not used) 


                # compute kernel...
                matrix ortho $p mdsc ker


                unset mdsc ;# (to save memory)

                # ...and compute quotient
                matrix quot $p ker $mdsn
                unset mdsn ;# (to save memory)            

                
                set ngen [lindex [matrix dimensions $ker] 0]
                
                # go to next round if no new gens needed:
                if {!$ngen} continue 
                
                set tdim [expr {$ideg-$sdeg}]
                if {$::viewtypeeven} {set tdim [expr {$ideg/2-$sdeg}]}
                rec {
                    catch {close $ch}
                    set logfile log.$sdeg.$edeg.$tdim
                    set ch [open $logfile w]
                    puts $ch "# data for s=$sdeg e=$edeg i=$ideg tdim=$tdim profile=$theprofile"
                    puts $ch "# full dimensions = $alldims = prev curr next"
                    puts $ch "# number of new generators = $ngen"
                    puts $ch "# initial homology calculation $nr -> $nc -> $cc"
                    puts $ch "x y label"
                    puts $ch "$nc $cc hom$sdeg.$edeg.$ideg"
                    puts $ch "$nr $nc moh$sdeg.$edeg.$ideg"
                }
                # Ok, we need $ngen new generators, and so far we only know the
                # signature-zero part of their differentials. 

                set id [llength [C$sn cget -genlist]]

                dbgclear

                set genlist {}
                set newdiffs {}
                for {set i 0} {$i<$ngen} {incr i} {
                    set vct [lindex [matrix extract row $ker $i] 0]
                    lappend newdiffs [C$sc decode $vct]
                    lappend genlist $id
                    incr id
                }

                unset -nocomplain vct ;# to save memory
                unset -nocomplain ker ;# to save memory

                dbgadd { "approximate diffs = $newdiffs" }
                dbgadd { "tridegree = $tridegree" }
                dbgadd { "profile = $theprofile" }
                
                # We use an auxiliary enumerator as a reference for error vectors. 
                # This one agrees with C$sp except that it has empty profile

                enumerator errenu
                foreach parm [C$sp configure] { 
                    errenu configure [lindex $parm 0] [lindex $parm 1] 
                }
                errenu configure -profile {}


                # these counters are used for error detection  
                set dimcnt  [C$sp dim]
                set fulldim [errenu dim]

                # The rows in the matrix errterms are "d(d($id))" so they
                # should be zero at the end of the correction process

                set errterms [steenrod::ComputeImage d$sc errenu $newdiffs]

                dbgadd { "errterms = $errterms" }
                dbgadd { "C$sp basis = [errenu basis]" }

                dbgadd { [lindex [list [C$sc con -profile {}] \
                                      "C$sc basis = [C$sc basis]" \
                                      [C$sc con -profile $theprofile]] 1] }

                dbgadd { "siglist = [C$sp siglist]" }

                # now correct errorterms by induction on signature

                while {[C$sp signext]} {

                    set sig [C$sp cget -signature]
                    C$sc configure -signature $sig 
                    C$sn configure -signature $sig 
                    
                    incr dimcnt [C$sp dim]

                    dbgadd { "signature $sig: (C$sc bas = [C$sc bas])" }
                    
                    # extract errors of this signature:
                    set seqmap [errenu seqno C$sp]
                    set errmat [matrix extract cols $errterms $seqmap]

                    dbgadd { "    extracted $errmat via $seqmap" }

                    # compute relevant part of matrix   C$sc -> C$sp 
                    set mdsn [eval steenrod::ComputeMatrix C$sc d$sc C$sp]

                    foreach {nr nc} [matrix dimensions $mdsn] break
                    set matliftdim [format $matliftdimfmt $nr $nc]

                    rec {
                        puts $ch "$nr $nc lft$sdeg.$edeg.$ideg"
                    }

                    dbgadd { "    matrix is $mdsn" }

                    # compute lift
                    set lft [matrix liftvar $p mdsn errmat]

                    dbgadd { "    lift is $lft" }

                    set cnt 0
                    set corrections {}
                    foreach id $genlist {
                        # TODO: use something better than "lindex" here!  
                        set preim [lindex [matrix extract row $lft $cnt] 0]
                        lappend corrections [set aux [C$sc decode $preim -1]]
                        set aux2 [lindex $newdiffs $cnt]
                        lset newdiffs $cnt [poly append $aux2 $aux]
                        incr cnt
                    }

                    dbgadd { "    using corrections $corrections" }
                    # update error terms
                    matrix addto errterms [steenrod::ComputeImage \
                                               d$sc errenu $corrections] 1 $p
                }

                # check that signature decomposition was complete 
                if {$fulldim != $dimcnt} {
                    dbgadd { "fulldim = $fulldim, dimcnt = $dimcnt" }
                    dbgprint
                    error "signatures not correctly enumerated"
                }

                # check that error terms really are zero
                if {![matrix iszero $errterms]} {
                    dbgadd { "final state: errterms=$errterms, newdiffs = $newdiffs" }
                    dbgprint
                    #vwait forever
                    error "error terms not reduced to zero"
                }

                # try to free some memory 
                unset -nocomplain mdsn 
                unset -nocomplain lft
                unset -nocomplain preim
                unset -nocomplain seqmap
                unset -nocomplain errmat
                unset -nocomplain errterms
                unset -nocomplain corrections
                rename errenu ""

                # introduce new generators:

                set gl [C$sn cget -genlist]
                foreach id $genlist df $newdiffs {
                    lappend gl [list $id $ideg $edeg 0]
                    eval d$sn set \[list 0 0 0 $id\] \$df
                    newgen $id [expr $sn] $edeg $ideg $df
                    rec {
                        puts $ch "# gen $id diff = [llength $df] summands"
                    }
                }
                eval C$sn configure -genlist \$gl

            }
        }

    }

    if {$usegui} break

    if {[incr repcnt -1]} {
        # memory debugging 
        flush stderr
        puts stderr "\# next round!" 
        puts stderr "polCount = $steenrod::_polCount"
        puts stderr "monCount = $steenrod::_monCount"
        puts stderr "matCount = $steenrod::_matCount"
        puts stderr "vecCount = $steenrod::_vecCount"
        puts stderr "objCount = $steenrod::_objCount"
        catch {
            puts stderr [format "memory   = %s kB" \
                             [lindex [exec egrep VmRSS [file join /proc [pid] status]] 1]]
        }
    }

}

finito

if 0 {
    # useful for debugging memory leaks: explicitly destroy everything 
    foreach v [info var] { catch {unset $v} }
    foreach c [info commands *] {
        switch -- $c {
            "switch" - "if" - "catch" - "foreach" - "rename" {}
            default { catch { rename $c "" } }
        }
    }
}
