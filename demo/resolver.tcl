#!/usr/bin/tclsh
#
# Resolve an algebra and display the results in a canvas
#
# Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
#
#  $Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

lappend auto_path ../lib

package require Steenrod

array set options {
    -prime   2
    -algebra {}
    -usegui  0
    -maxdim  40
    -maxs    50
}

foreach {opt val} $argv {
    if {![info exist options($opt)]} {
        puts [join [list "unknown option $opt" \
                        "available options: [array names options]"] ", "]
        puts "Example: for A(2) use -algebra \"0 0 {3 2 1} 0\""
        exit 1
    }
    set options($opt) $val
}

# see if options make sense
poly::enumerator x 
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

if {![scan $options(-maxdim) %d maxdim]} { 
    puts "expected integer, found $options(-maxdim)" 
    exit 1
}

if {![scan $options(-maxs) %d maxs]} { 
    puts "expected integer, found $options(-maxs)" 
    exit 1
}

if {![scan $options(-usegui) %d usegui]} { 
    puts "expected integer, found $options(-usegui)" 
    exit 1
}

# debugging aids: 

set dbgflag 0
set errinf {}

proc dbgclear {} { 
    global errinf
    set errinf {}
}

proc dbgprint {} {
    global errinf dbgflag
    if {!$dbgflag} { 
        puts "debug logging not enabled (set dbgflag)"
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

# create empty modules and maps

incr maxs
for {set i -2} {$i<=$maxs} {incr i} {
    poly::enumerator C$i -prime $p -algebra $alg
    poly::monomap d$i
}

# fake generator from Ext^0
C0 configure -genlist 0 

proc matprint {name mat} {
    if 0 {
        set ind "\n    "
        puts -nonewline "$name ([linalg::getdims $mat])=$ind"
        puts [join $mat $ind]
    }
    if 0 {
        puts "$name: [::linalg::getdims $mat]"
    }
}

set maxe $maxs

# newgen is a callback function that can be overwritten in the GUI.
# The GUI can also put a trace on the tridegree and theprofile variables. 
# Finally, when we're through we'll call the finito proc.

set tridegree  {} 
set theprofile {}

proc finito {} { puts "Done." }

proc newgen {id s e i diff} {
    puts [format "%3d / %3d : gen %d (pro $::theprofile) (diff = %s)" $s $i $id $diff]
}

if {$usegui} {
    source [file join [file dirname [info script]] simplegui.tcl]
}

# Routines that determine the maximal allowed profile 
#

proc maxUpperProfile {prime algebra s ideg edeg} {
    incr s 0

    set edegs [prime::extdegs $prime]
    set rdegs [prime::reddegs $prime]
    set tpmo  [prime::tpmo $prime]

    set edat 0
    set rdat {}

    foreach dg $rdegs {
        set slope [expr $tpmo * $dg]
        if {[expr $slope * $s > $ideg]} { 
            lappend rdat 666
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
    incr s 1

    if {$algebra=={}} { set algebra {0 -1 {666 666 666 666 666 666 666} 0} }
    
    set ae [lindex $algebra 1]
    set ar [lindex $algebra 2]

    set edegs [prime::extdegs $prime]
    set rdegs [prime::reddegs $prime]
    set tpmo  [prime::tpmo $prime]

    # ext and red describe the profile of the used subalgebra B
    set ext 0
    set red {0 0 0 0 0 0 0}

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

# Main loop starts here:

newgen 0 -1 0 0 {} ;# fake gen of deg zero
    
    for {set sdeg 0} {$sdeg<$maxs} {incr sdeg} {

        set maxi [expr $maxdim + $sdeg]
        
        for {set ideg $sdeg} {$ideg<=$maxi} {incr ideg} {
            for {set edeg 0} {($edeg<=[expr $sdeg+1]) && ($edeg<=$maxe)} {incr edeg} {
         
            # resolve tridegree (s,e,t) == ($sdeg, $edeg, $ideg)
            
            # determine previous, current, and next value of s
            set sp [expr $sdeg-1]
            set sc $sdeg
            set sn [expr $sdeg+1]
            
            foreach hdg [list $sp $sc $sn] {
                C$hdg configure -edeg $edeg -ideg $ideg 
            }

            # find maximal possible profiles  
            set pupper [maxUpperProfile $p $alg $sdeg $ideg $edeg]
            set plower [maxLowerProfile $p $alg $sdeg $ideg $edeg]

            # count dimensions
            C$sdeg configure -profile $pupper ; set updim [C$sdeg dimension]
            C$sdeg configure -profile $plower ; set lodim [C$sdeg dimension]

            if 0 {
                puts "(s,i)=($sdeg,$ideg), t-s=[expr $ideg-$sdeg]"
                puts "lowprof = $plower, dim = $lodim"
                puts "uppprof = $pupper, dim = $updim"
            }

            if {(0==$updim) || (0==$lodim)} continue

            if {$updim > $lodim} { 
                set theprofile $plower
            } else {
                set theprofile $pupper
            }
            
            # at this point the gui's trace will trigger
            set tridegree [list $sdeg $ideg $edeg]

#            puts "(s:$sdeg, i:$ideg, e:$edeg) : profile $theprofile"

            foreach mod [list C$sp C$sc C$sn] { 
                $mod configure -profile $theprofile 
                $mod sigreset
               # puts "$mod config = [$mod config]"
               # puts "$mod basis = [$mod basis]"
            }

            # now compute matrices   mdsc : Cs -> Cs-1  and  mdsn : Cs+1 -> Cs

            if {0 == $sc} {
                # fake appropriate zero matrix
                if {($edeg==0) && ($ideg==0)} {
                    set mdsc 1
                } else {
                    # here C-1 = F_p = 0. create a zero matrix with the
                    # right number of rows.
                    set mdsc {}
                    for {set i [C0 dim]} {$i>0} {incr i -1} { lappend mdsc 0 }
                }                
                eval set mdsn \[poly::ComputeMatrix C$sn d$sn C$sc\]  
            } else {
                # using eval here gives nicer error reports
                eval set mdsc \[poly::ComputeMatrix C$sc d$sc C$sp\]  
                eval set mdsn \[poly::ComputeMatrix C$sn d$sn C$sc\]  
            }
            
            # compute image...
            linalg::ortho $p mdsn ker  ;# (ker is not used) 

            # compute kernel...
            linalg::ortho $p mdsc ker

            unset mdsc ;# (to save memory)

            # ...and compute quotient
            linalg::quot $p ker mdsn

            unset mdsn ;# (to save memory)
            
            set ngen [lindex [linalg::getdims $ker] 0]
            
            # go to next round if no new gens needed:
            if {!$ngen} continue 
            
            # Ok, we need $ngen new generators, and so far we only know the
            # signature-zero part of their differentials. 

            set id [llength [C$sn cget -genlist]]

            array unset newdiffs
            array unset errorterms
            set genlist {}

            dbgclear

            set newdiffs {}
            for {set i 0} {$i<$ngen} {incr i} {
                set vct [lindex [linalg::extract row $ker $i] 0]
                lappend newdiffs [C$sc decode $vct]
                lappend genlist $id
                incr id
            }

            dbgadd { "approximate diffs = $newdiffs" }
            dbgadd { "tridegree = $tridegree" }
            dbgadd { "profile = $theprofile" }
            
            # We use an auxiliary enumerator as a reference for error vectors. 
            # This one agrees with C$sp except that it has empty profile

            poly::enumerator errenu
            foreach parm [C$sp configure] { 
                errenu configure [lindex $parm 0] [lindex $parm 1] 
            }
            errenu configure -profile {}

            # these counters are used for error detection  
            set dimcnt  [C$sp dim]
            set fulldim [errenu dim]

            # The rows in the matrix errterms are "d(d($id))" so they
            # should be zero at the end of the correction process

            set errterms [poly::ComputeImage d$sc errenu $newdiffs]
            
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
                set errmat [linalg::extract cols $errterms $seqmap]

                dbgadd { "    extracted $errmat via $seqmap" }

                # compute relevant part of matrix   C$sc -> C$sp 
                set mdsn [eval poly::ComputeMatrix C$sc d$sc C$sp]

                dbgadd { "    matrix is $mdsn" }

                # compute lift
                set lft [linalg::lift $p mdsn errmat]

                dbgadd { "    lift is $lft" }

                set cnt 0
                set corrections {}
                foreach id $genlist preim $lft {
                    lappend corrections [set aux [C$sc decode $preim -1]]
                    set aux2 [lindex $newdiffs $cnt]
                    lset newdiffs $cnt [poly::append $aux2 $aux]
                    incr cnt
                }

                dbgadd { "    using corrections $corrections" }

                # update error terms
                set errterms [linalg::addmatrix \
                                  $errterms [poly::ComputeImage \
                                                 d$sc errenu $corrections] $p]
            }
                
            # check that signature decomposition was complete 
            if {$fulldim != $dimcnt} {
                dbgadd { "fulldim = $fulldim, dimcnt = $dimcnt" }
                dbgprint
                error "signatures not correctly enumerated"
            }

            # check that error terms really are zero
            if 1 {
                foreach row $errterms {
                    foreach entry $row {
                        if {$entry} {
                            dbgadd { "final state: errterms=$errterms, newdiffs = $newdiffs" }
                            dbgprint
#vwait forever
                            error "errorterms not reduced to zero"
                        }
                    }
                }
            }

            # introduce new generators:

            set gl [C$sn cget -genlist]
            foreach id $genlist df $newdiffs {
                lappend gl [list $id $ideg $edeg 0]
                eval d$sn set \[list 0 0 0 $id\] \$df
                newgen $id [expr $sn-1] $edeg $ideg $df
            }
            eval C$sn configure -genlist \$gl

        }

    }

        if {$sdeg>2} {
            rename C[expr $sdeg-3] ""
            rename d[expr $sdeg-3] ""
        }

}

finito
