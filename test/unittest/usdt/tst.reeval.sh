#!/bin/bash
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2007 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="-I${PWD}/uts/common"

mkdir $tmpdir/usdt-reeval
cd $tmpdir/usdt-reeval

cat > test.c <<EOF
#include <sys/sdt.h>

int
main(int argc, char **argv)
{
	DTRACE_PROBE(test_prov, zero);
}
EOF

cat > prov.d <<EOF
provider test_prov {
	probe zero();
};
EOF

${CC} ${CFLAGS} -c test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi
$dtrace -G -s prov.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >& 2
	exit 1
fi
${CC} ${CFLAGS} -o test test.o prov.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >& 2
	exit 1
fi

script()
{
	$dtrace -wZs /dev/stdin <<EOF
	BEGIN
	{
		system("$tmpdir/usdt-reeval/test");
	}

	test_prov*:::
	{
		seen = 1;
	}

	proc:::exit
	/progenyof(\$pid) && execname == "test"/
	{
printf("seen = %d", seen);
		exit(seen ? 0 : 2);
	}
EOF
}

script
status=$?
echo $status

exit $status
