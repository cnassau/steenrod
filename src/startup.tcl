foreach steenrod::xfile [glob -d [file dirname [info script]] *.tc] {
   namespace eval :: [list source $steenrod::xfile]
}
if {$::steenrod::cl::enabled} {
   package require Steenrod::opencl 1.0
}
unset -nocomplain steenrod::xfile

