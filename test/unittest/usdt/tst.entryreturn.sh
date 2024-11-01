#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

# @@xfail: dtv2, no wildcard usdt probes yet

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="$test_cppflags"
LDFLAGS="$test_ldflags"

DIRNAME="$tmpdir/usdt-entryreturn.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > test.c <<EOF
#include <sys/sdt.h>

int
main(int argc, char **argv)
{
	DTRACE_PROBE(test_prov, entry);
	DTRACE_PROBE(test_prov, __entry);
	DTRACE_PROBE(test_prov, foo__entry);
	DTRACE_PROBE(test_prov, carpentry);
	DTRACE_PROBE(test_prov, miniatureturn);
	DTRACE_PROBE(test_prov, foo__return);
	DTRACE_PROBE(test_prov, __return);
	/*
	 * Unfortunately, a "return" probe is not currently possible due to
	 * the conflict with a reserved word.
	 */
	DTRACE_PROBE(test_prov, done);
}
EOF

cat > prov.d <<EOF
provider test_prov {
	probe entry();
	probe __entry();
	probe foo__entry();
	probe carpentry();
	probe miniatureturn();
	probe foo__return();
	probe __return();
	probe done();
};
EOF

${CC} ${CFLAGS} -c test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi
$dtrace $dt_flags -G -s prov.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >& 2
	exit 1
fi
${CC} ${LDFLAGS} -o test test.o prov.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >& 2
	exit 1
fi

script()
{
	$dtrace $dt_flags -wqZFs /dev/stdin <<EOF
	BEGIN
	{
		system("$DIRNAME/test");
		printf("\n");
	}

	test_prov*:::done
	/progenyof(\$pid)/
	{
		exit(0);
	}

	test_prov*:::
	/progenyof(\$pid)/
	{
		printf("\n");
	}
EOF
}

# script | cut -c5-
script
status=$?

exit $status
