#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, Oracle and/or its affiliates. All rights reserved.
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
#pragma D depends_on library libb.$$.d

inline int foo = bar;
EOF
        cat > $libdir/libb.$$.d <<EOF
#pragma D depends_on module doogle_knows_all_probes

inline int bar = 0xd0061e;
EOF
}


setup_libs

$dtrace $dt_flags -L$libdir -e

status=$?
rm -rf $libdir
exit $status
