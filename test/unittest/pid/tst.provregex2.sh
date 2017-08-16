#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that probes will be picked up after a dlopen(3C)
# when the pid provider is specified as a glob (e.g., p*d$target.)
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIR=${TMPDIR:-/tmp}/provregex2.$$

mkdir $DIR

cat > $DIR/Makefile <<EOF
all: main altlib.so

main: main.o
	cc -o main -ldl main.o

main.o: main.c
	cc -c main.c

altlib.so: altlib.o
	cc -z defs -shared -o altlib.so altlib.o

altlib.o: altlib.c
	cc -c altlib.c
EOF

cat > $DIR/altlib.c <<EOF
void
go(void)
{
}
EOF

cat > $DIR/main.c <<EOF
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>

void
go(void)
{
}

int
main(int argc, char **argv)
{
	void *alt;
	void *alt_go;

	go();

	if ((alt = dlopen("$DIR/altlib.so", RTLD_LAZY | RTLD_LOCAL)) 
	    == NULL) {
		printf("dlopen of altlib.so failed: %s\n", dlerror());
		return (1);
	}

	if ((alt_go = dlsym(alt, "go")) == NULL) {
		printf("failed to lookup 'go' in altlib.so\n");
		return (1);
	}

	((void (*)(void))alt_go)();

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
	@foo[probemod, probefunc, probename] = count();
}

END
{
	printa("%s:%s:%s %@u\n",@foo);
}
EOF

script() {
	$dtrace $dt_flags -q -s $DIR/main.d -c $DIR/main
}

script
status=$?

rm -rf $DIR

exit $status
