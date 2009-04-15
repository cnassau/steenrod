foreach steenrod::xfile [glob -d [file dirname [info script]] *.tc] {
   namespace eval :: [list source $steenrod::xfile]
}
unset -nocomplain steenrod::xfile

