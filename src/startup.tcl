foreach steenrod::xfile [glob -d [file dirname [info script]] *.tc] {
   namespace eval :: [list source $steenrod::xfile]
}
if {$::steenrod::cl::enabled} {
   package require Steenrod::opencl [package present Steenrod]
}
unset -nocomplain steenrod::xfile

