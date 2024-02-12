#!/bin/bash
set -ve
cd /src
mkdir build || true
cd build
../configure 
make
make install
TCLTEST_OPTIONS="-constraints BIGTEST" tclsh ../test/all.tcl
cd /usr/lib && tar czf /out/steenrod.tgz Steenrod*
echo "done"
