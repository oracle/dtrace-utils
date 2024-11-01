#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that USDT providers are removed when the associated
# load object is closed via dlclose(3dl).
#
# The expected sequence of events is:
#
#     main.c                                 script
#     ==============================         =================================
#
#     load livelib.so
#     write "started" to myfile.txt
#                                            see "started" in myfile.txt
#                                            check USDT provider test_prov$pid
#                                            signal USR1
#     get USR1
#     run dlclose()
#     write  "dlclosed" to myfile.txt
#                                            see "dlclosed" in myfile.txt
#                                            check USDT provider test_prov$pid
#                                            kill main.c
#
# The first USDT provider check should find test_prov$pid.
# The second should not.

dtrace=$1
CC=/usr/bin/gcc
CFLAGS=

DIRNAME="$tmpdir/usdt-dlclose1.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

#
# set up Makefile
#

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

#
# set up some small files
#

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

#
# set up main.c
#

cat > main.c <<EOF
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>

void *live;

static void
interrupt(int sig)
{
	/* close livelib.so to remove USDT probes */
	dlclose(live);

	/* notification */
	printf("dlclosed\n");
	fflush(stdout);
}

int
main(int argc, char **argv)
{
	struct sigaction act;

	/* open livelib.so to make USDT probes visible */
	if ((live = dlopen("./livelib.so", RTLD_LAZY | RTLD_LOCAL)) == NULL) {
		printf("dlopen of livelib.so failed: %s\n", dlerror());
		return 1;
	}

	/* set the handler to listen for SIGUSR1 */
	act.sa_handler = interrupt;
	act.sa_flags = 0;
	if (sigaction(SIGUSR1, &act, NULL)) {
		printf("set handler failed\n");
		return 1;
	}

	/* notification */
	printf("started\n");
	fflush(stdout);

	/* wait for SIGUSR1 to execute handler */
	pause();

	/* wait for SIGKILL to terminate */
	pause();

	return 0;
}
EOF

#
# make
#

make > /dev/null
if [ $? -ne 0 ]; then
	echo "failed to build" >& 2
	exit 1
fi

#
# set up a wait mechanism
#

# args:
#   1: number of seconds to wait
#   2: string to watch for
#   3: file to watch
function mywait() {
	for iter in `seq $1`; do
		sleep 1
		if grep -q "$2" $3; then
			iter=0
			break
		fi
	done
	if [[ $iter -ne 0 ]]; then
		echo did not see message: $2
		echo "==================== start of messages"
		cat $3
		echo "==================== end of messages"
		kill -s KILL $pid
		exit 1
	fi
}

#
# run the process in the background and check USDT probes
#

./main > myfile.txt &
pid=$!
disown %+
echo started pid $pid

mywait 6 "started"  myfile.txt # wait for process to start
$dtrace $dt_flags -lP test_prov$pid      # check USDT probes
kill -s USR1 $pid              # signal process

mywait 6 "dlclosed" myfile.txt # wait for process to dlclose
$dtrace $dt_flags -lP test_prov$pid      # check USDT probes
kill -s KILL $pid              # kill process

exit 0
