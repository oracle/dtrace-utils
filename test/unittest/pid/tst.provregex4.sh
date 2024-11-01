#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that USDT probes will be picked up after a dlopen(3C)
# when a regex in the provider name matches both USDT probes and pid probes
# (e.g., p*d$target matches both pid$target and pyramid$target.)
#
# @@xfail: dtv2

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIR=$tmpdir/provregex4.$$

mkdir $DIR

cat > $DIR/Makefile <<EOF
all: main altlib.so

main: main.o provmain.o
	cc $test_cppflags -o main main.o provmain.o -ldl

main.o: main.c prov.h
	cc $test_cppflags -c main.c

prov.h: prov.d
	$dtrace $dt_flags -h -s prov.d

provmain.o: prov.d main.o
	$dtrace $dt_flags -G -o provmain.o -s prov.d main.o

altlib.so: altlib.o provalt.o
	cc $test_cppflags -z defs --shared -o altlib.so altlib.o provalt.o

altlib.o: altlib.c prov.h
	cc $test_cppflags -c altlib.c

provalt.o: prov.d altlib.o
	$dtrace $dt_flags -G -o provalt.o -s prov.d altlib.o
EOF

cat > $DIR/prov.d <<EOF
provider pyramid {
	probe entry();
};
EOF

cat > $DIR/altlib.c <<EOF
#include <sys/sdt.h>
#include "prov.h"

void
go(void)
{
	PYRAMID_ENTRY();
}
EOF

cat > $DIR/main.c <<EOF
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/sdt.h>
#include "prov.h"

void
go(void)
{
	PYRAMID_ENTRY();
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
		return 1;
	}

	if ((alt_go = dlsym(alt, "go")) == NULL) {
		printf("failed to lookup 'go' in altlib.so\n");
		return 1;
	}

	((void (*)(void))alt_go)();

	return 0;
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
