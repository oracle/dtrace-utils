#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

libdir=$tmpdir/libdep.$$
dtrace=$1

setup_libs()
{
        mkdir $libdir
        cat > $libdir/liba.$$.d <<EOF
#pragma D depends_on library libb.$$.d
#pragma D depends_on library libc.$$.d
#pragma D depends_on library libd.$$.d
EOF
        cat > $libdir/libb.$$.d <<EOF
#pragma D depends_on library libc.$$.d
EOF
        cat > $libdir/libc.$$.d <<EOF
EOF
        cat > $libdir/libd.$$.d <<EOF
EOF
        cat > $libdir/libe.$$.d <<EOF
#pragma D depends_on library liba.$$.d
EOF
        cat > $libdir/libf.$$.d <<EOF
EOF
}


setup_libs

$dtrace $dt_flags -L$libdir -e -n 'BEGIN {exit(0)}'

status=$?
rm -rf $libdir
exit $status
