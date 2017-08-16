#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that specifying a glob in a pid provider name
# (e.g., p*d$target) works.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIR=${TMPDIR:-/tmp}/provregex1.$$

mkdir $DIR

cat > $DIR/Makefile <<EOF
all: main

main: main.o
	cc -o main main.o

main.o: main.c
	cc -c main.c
EOF

cat > $DIR/main.c <<EOF
void
go(void)
{
}

int
main(int argc, char **argv)
{
	go();

	return (0);
}
EOF

make -C $DIR > /dev/null
if [ $? -ne 0 ]; then
	echo "failed to build" >&2
	exit 1
fi

cat > $DIR/main.d <<'EOF'
p*d$target::go:entry
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
