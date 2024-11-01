#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@skip: not ported

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS=

DIR="$tmpdir/misc-include.$$.$RANDOM"
mkdir $DIR

doit()
{
	file=$1
	ofile=$2
	errfile=$3
	cfile=$DIR/inc.$$.$file.c
	cofile=$DIR/inc.$$.$file
	cat > $cfile <<EOF
#include <sys/$file>
void
main()
{}
EOF
	if $CC $test_cppflags $CFLAGS -o $cofile $cfile >/dev/null 2>&1; then
		$dtrace $dt_flags -xerrtags -C -s /dev/stdin \
		    >/dev/null 2>$errfile <<EOF
#include <sys/$file>
BEGIN
{
	exit(0);
}
EOF
		if [ $? -ne 0 ]; then
			echo $inc failed: `cat $errfile | head -1` > $ofile
		else
			echo $inc succeeded > $ofile
		fi
		rm -f $errfile
	fi

	rm -f $cofile $cfile 2>/dev/null
}

if [ ! -x $CC ]; then
	echo "$0: bad compiler: $CC" >& 2
	exit 1
fi

concurrency=`grep processor /proc/cpuinfo | wc -l`
let concurrency=concurrency*4
let i=0

files=/usr/include/sys/*.h

#
# There are a few files in /usr/include/sys that are known to be bad -- usually
# because they include static globals (!) or function bodies (!!) in the header
# file.  Hopefully these remain sufficiently few that the O(#files * #badfiles)
# algorithm, below, doesn't become a problem.  (And yes, writing scripts in
# something other than ksh1888 would probably be a good idea.)  If this script
# becomes a problem, kindly fix it by reducing the number of bad files!  (That
# is, fix it by fixing the broken file, not the broken script.)
#
badfiles="ctype.h eri_msg.h ser_sync.h sbpro.h neti.h hook_event.h \
    bootconf.h bootstat.h dtrace.h dumphdr.h exacct_impl.h fasttrap.h \
    kobj.h kobj_impl.h ksyms.h lockstat.h smedia.h stat.h utsname.h"

for inc in $files; do
	file=`basename $inc`
	for bad in $badfiles; do
		if [ "$file" = "$bad" ]; then
			continue 2 
		fi
	done

	ofile=$DIR/inc.$$.$file.out
	errfile=$DIR/inc.$$.$file.err
	doit $file $ofile $errfile &
	let i=i+1

	if [ $i -eq $concurrency ]; then
		#
		# This isn't optimal -- it creates a highly fluctuating load
		# as we wait for all work to complete -- but it's an easy
		# way of parallelizing work.
		#
		wait
		let i=0
	fi
done

wait

bigofile=$DIR/inc.$$.out

for inc in $files; do
	file=`basename $inc`
	ofile=$DIR/inc.$$.$file.out

	if [ -f $ofile ]; then
		cat $ofile >> $bigofile
		rm $ofile
	fi
done

status=$(grep "failed:" $bigofile | wc -l)
cat $bigofile
rm -f $bigofile
exit $status
