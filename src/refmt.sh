#!/bin/sh

for i in $* ; do 
    sed "s/\t/    /" $i | sed "s/( /(/" | sed "s/ )/)/" > refmt.aux
    diff refmt.aux $i > /dev/null
 
    if [ $? -ge 1 ]
	then
	echo reformatted $i
	mv $i $i.bak
	cp refmt.aux $i
    fi
    
    rm refmt.aux
done;
