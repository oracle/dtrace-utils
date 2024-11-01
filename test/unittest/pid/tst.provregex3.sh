#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that a regex in the provider name will match
# USDT probes as well as pid probes (e.g., p*d$target matches both 
# pid$target and pyramid$target.)

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIR=$tmpdir/provregex3.$$

mkdir $DIR

cat > $DIR/Makefile <<EOF
all: main

main: main.o prov.o
	cc $test_cppflags -o main main.o prov.o

main.o: main.c prov.h
	cc $test_cppflags -c main.c

prov.h: prov.d
	$dtrace $dt_flags -h -s prov.d

prov.o: prov.d main.o
	$dtrace $dt_flags -G -64 -s prov.d main.o
EOF

cat > $DIR/prov.d <<EOF
provider pyramid {
	probe entry();
};
EOF

cat > $DIR/main.c <<EOF
#include <sys/sdt.h>
#include "prov.h"

int
main(int argc, char **argv)
{
	PYRAMID_ENTRY();
}
EOF

make -C $DIR > /dev/null
if [ $? -ne 0 ]; then
	echo "failed to build" >&2
	exit 1
fi

cat > $DIR/main.d <<'EOF'
p*d$target::main:entry
{
	printf("%s:%s:%s\n", probemod, probefunc, probename);
}
EOF

script() {
	$dtrace $dt_flags -q -s $DIR/main.d -c $DIR/main
}

script
status=$?

rm -rf $DIR

exit $status
