#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@xfail: dtv2, no wildcard usdt probes across pids yet
#
PATH=/usr/bin:/usr/sbin:$PATH

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS=

DIRNAME="$tmpdir/usdt-dlclose2.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > Makefile <<EOF
all: main livelib.so deadlib.so

main: main.o prov.o
	\$(CC) \$(test_ldflags) -o main main.o -ldl

main.o: main.c
	\$(CC) \$(test_cppflags) -c main.c


livelib.so: livelib.o prov.o
	\$(CC) \$(test_ldflags) -shared -o livelib.so livelib.o prov.o -lc

livelib.o: livelib.c prov.h
	\$(CC) \$(test_cppflags) -c livelib.c

prov.o: livelib.o prov.d
	$dtrace \$(dt_flags) -G -s prov.d livelib.o

prov.h: prov.d
	$dtrace \$(dt_flags) -h -s prov.d


deadlib.so: deadlib.o
	\$(CC) \$(test_ldflags) -shared -o deadlib.so deadlib.o -lc

deadlib.o: deadlib.c
	\$(CC) \$(test_cppflags) -c deadlib.c

clean:
	rm -f main.o livelib.o prov.o prov.h deadlib.o

clobber: clean
	rm -f main livelib.so deadlib.so
EOF

cat > prov.d <<EOF
provider test_prov {
	probe go();
};
EOF

cat > livelib.c <<EOF
#include "prov.h"

void
go(void)
{
	TEST_PROV_GO();
}
EOF

cat > deadlib.c <<EOF
void
go(void)
{
}
EOF


cat > main.c <<EOF
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	void *live, *dead;
	void *go;

	if ((live = dlopen("./livelib.so", RTLD_LAZY | RTLD_LOCAL)) == NULL) {
		printf("dlopen of livelib.so failed: %s\n", dlerror());
		return 1;
	}

	dlclose(live);

	if ((dead = dlopen("./deadlib.so", RTLD_LAZY | RTLD_LOCAL)) == NULL) {
		printf("dlopen of deadlib.so failed: %s\n", dlerror());
		return 1;
	}

	if ((live = dlopen("./livelib.so", RTLD_LAZY | RTLD_LOCAL)) == NULL) {
		printf("dlopen of livelib.so failed: %s\n", dlerror());
		return 1;
	}

	if ((go = dlsym(live, "go")) == NULL) {
		printf("failed to lookup 'go' in livelib.so\n");
		return 1;
	}

	((void (*)(void))go)();

	return 0;
}
EOF

make > /dev/null
if [ $? -ne 0 ]; then
	echo "failed to build" >& 2
	exit 1
fi

script() {
	$dtrace $dt_flags -c ./main -Zqs /dev/stdin <<EOF
	test_prov*:::
	{
		printf("%s:%s:%s\n", probemod, probefunc, probename);
	}
EOF
}

script
status=$?

exit $status
