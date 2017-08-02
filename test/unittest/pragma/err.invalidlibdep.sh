#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

libdir=${TMPDIR:-/tmp}/libdep.$$
dtrace=$1

setup_libs()
{
	mkdir $libdir
	cat > $libdir/liba.$$.d <<EOF
#pragma D depends_on library libnotthere.$$.d
EOF
	cat > $libdir/libb.$$.d <<EOF
#pragma D depends_on library libc.$$.d
EOF
	cat > $libdir/libc.$$.d <<EOF
#pragma D depends_on library liba.$$.d
EOF
}


setup_libs

$dtrace $dt_flags -L$libdir -e

status=$?
rm -rf $libdir
exit $status
