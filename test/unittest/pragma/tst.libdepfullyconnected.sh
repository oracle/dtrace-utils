#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that we can generate the correct ordering for
# a given dependency specification. All files either have a dependency
# on another file or are the dependent of another file. In this way we
# guarantee consistent ordering as no nodes in the dependency graph will
# be isolated.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

tmpfile=$tmpdir/libdeporder.$$
libdir=$tmpdir/libdep.$$
dtrace=$1

setup_libs()
{
	mkdir $libdir
	cat > $libdir/liba.$$.d <<EOF
#pragma D depends_on library libd.$$.d
EOF
	cat > $libdir/libb.$$.d <<EOF
EOF
	cat > $libdir/libc.$$.d <<EOF
#pragma D depends_on library libe.$$.d
EOF
	cat > $libdir/libd.$$.d <<EOF
#pragma D depends_on library libb.$$.d
EOF
	cat > $libdir/libe.$$.d <<EOF
#pragma D depends_on library liba.$$.d
EOF
}


setup_libs

DTRACE_DEBUG=1 $dtrace $dt_flags -L$libdir -e -n 'BEGIN {exit(0)}' >$tmpfile 2>&1

perl /dev/stdin $tmpfile <<EOF

	@order = qw(libc libe liba libd libb);

	while (<>) {
		if (\$_ =~ /lib[a-e]\.[0-9]+.d/) {
			\$pos = length \$_;
			for (\$i=0; \$i<=1;\$i++) {
				\$pos = rindex(\$_, "/", \$pos);
				\$pos--;
			}

			push(@new, substr(\$_, \$pos+2, 4));
			next;
		}
		next;
	}

	exit 1 if @new != @order;

	while (@new) {
		exit 1 if pop(@new) ne pop(@order);
	}

	exit 0;
EOF


status=$?
rm -rf $libdir
rm $tmpfile
exit $status
