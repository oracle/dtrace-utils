#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
PATH=/usr/bin:/usr/sbin:$PATH

#
# In this test, we send alternating USR1 and USR2 signals to an executable
# that responds by opening and closing, respectively, a shared library with
# USDT probes.  After each signal, we check "dtrace -l" to confirm that the
# USDT probes are and are not listed, as expected.
#

dtrace=$1
CC=/usr/bin/gcc
CFLAGS=

DIRNAME="$tmpdir/usdt-dlclose4.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

#
# Set up the source files.
#

cat > Makefile <<EOF
all: main livelib.so

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

clean:
	rm -f main.o livelib.o prov.o prov.h

clobber: clean
	rm -f main livelib.so
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

cat > main.c <<EOF
#include <dlfcn.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void *live;

/*
 * Open and close livelib.so, thereby adding or removing USDT probes.
 */

static void my_open(int sig) {
	live = dlopen("./livelib.so", RTLD_LAZY | RTLD_LOCAL);
	if (live == NULL) {
		printf("dlopen of livelib.so failed: %s\n", dlerror());
		exit(1);
	}
}

static void my_close(int sig) {
	dlclose(live);
}

int
main(int argc, char **argv)
{
	struct sigaction act;

	/*
	 * Set USR1 (USR2) to open (close) the livelib.so.
	 */
	act.sa_flags = 0;
	act.sa_handler = my_open;
	if (sigaction(SIGUSR1, &act, NULL)) {
		printf("set handler failed\n");
		return 1;
	}
	act.sa_handler = my_close;
	if (sigaction(SIGUSR2, &act, NULL)) {
		printf("set handler failed\n");
		return 1;
	}

	/*
	 * Listen for signals.
	 */
	while (pause() == -1)
		;

	return 0;
}
EOF

#
# Build.
#

make > /dev/null
if [ $? -ne 0 ]; then
	echo "failed to build" >& 2
	exit 1
fi

# Define a function that looks for the USDT probe with "dtrace -l".
# For debugging, one could also check:
#     ls /run/dtrace/probes/$pid/test_prov$pid/livelib.so/go/go
#     ls /run/dtrace/stash/dof-pid/$pid/*/parsed/test_prov:livelib.so:go:go

function check_USDT_probes() {
	$dtrace $dt_flags -lP test_prov$pid |& awk '
	    /ID *PROVIDER *MODULE *FUNCTION *NAME/ { next }
	    /test_prov'$pid' *livelib\.so *go *go/ { exit(0) }
	    /No probe matches description/ { exit(1) }'
	return $?
}

# Define a function that checks loading the library:
# send USR1 and wait up to 6 seconds for the USDT probe to appear.

function load_lib() {
	kill -s USR1 $pid
	for iter in `seq 6`; do
		sleep 1
		if check_USDT_probes; then
			iter=0
			break
		fi
	done
	if [[ $iter -ne 0 ]]; then
		echo did not see USDT probe
		kill -s KILL $pid
		exit 1
	fi
	echo as expected: USDT probe appeared
}

# Define a function that checks unloading the library:
# send USR2 and wait up to 6 seconds for the USDT probe to disappear.

function unload_lib() {
	kill -s USR2 $pid  # send USR2 to unload library and USDT probe
	for iter in `seq 6`; do
		sleep 1
		if ! check_USDT_probes; then
			iter=0
			break
		fi
	done
	if [[ $iter -ne 0 ]]; then
		echo still see USDT probe after timeout
		kill -s KILL $pid
		exit 1
	fi
	echo as expected: USDT probe disappeared
}

# Start the process.

./main &
pid=$!
sleep 2

# Check.

load_lib
unload_lib
load_lib
unload_lib

# Clean up.

kill -s KILL $pid
wait $pid >& /dev/null

echo success
exit 0
