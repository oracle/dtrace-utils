#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@timeout: 80

PATH=/usr/bin:/usr/sbin:$PATH

#
# In this test, we check that is-enabled probes depend correctly on pid.
#

dtrace=$1
CC=/usr/bin/gcc
CFLAGS=

DIRNAME="$tmpdir/usdt-enable_pid.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

#
# Set up the source files.
#

cat > prov.d <<EOF
provider test_prov {
	probe go();
};
EOF

cat > main.c <<EOF
#include <signal.h>
#include <stdio.h>
#include "prov.h"

/* We check if the is-enabled probe is or is not enabled (or unknown). */
#define ENABLED_IS	1
#define ENABLED_NOT	2
#define ENABLED_UNK	3

/* Start with the previous probe "unknown". */
int prv = ENABLED_UNK;
long long num = 0;

/* Report how many times the previous case was encountered. */
static void report() {

	/* Skip if there is nothing to report. */
	if (num == 0)
		return;

	switch (prv) {
	case ENABLED_IS:
		printf("is enabled\n");
		break;
	case ENABLED_NOT:
		printf("is not enabled\n");
		break;
	}
	fflush(stdout);

	/* Reset. */
	prv = ENABLED_UNK;
	num = 0;
}

/* When USR1 is received, mark an "epoch" in the output. */
static void mark_epoch(int sig) {
	report();
	printf("=== epoch ===\n");
	fflush(stdout);
}

int
main(int argc, char **argv)
{
	struct sigaction act;

	/* Set USR1 to mark epochs. */
	act.sa_flags = 0;
	act.sa_handler = mark_epoch;
	if (sigaction(SIGUSR1, &act, NULL)) {
		printf("set handler failed\n");
		return 1;
	}

	/* Just keep looping, counting consecutive cases. */
	while (1) {
		int now;

		/* Check the is-enabled probe. */
		if (TEST_PROV_GO_ENABLED()) {
			now = ENABLED_IS;
		} else {
			now = ENABLED_NOT;
		}

		/* Compare to the previous case. */
		if (now == prv) {
			num++;
		} else {
			report();  /* resets num */
			prv = now;
		}
	}

	return 0;
}
EOF

#
# Build the test program.
#

$dtrace $dt_flags -h -s prov.d
if [ $? -ne 0 ]; then
	echo "failed to generate header file" >&2
	exit 1
fi
cc $test_cppflags -c main.c
if [ $? -ne 0 ]; then
	echo "failed to compile test" >&2
	exit 1
fi
$dtrace $dt_flags -G -64 -s prov.d main.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >&2
	exit 1
fi
cc $test_cppflags -o main main.o prov.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >&2
	exit 1
fi

#
# Start two copies.
#

./main >& out.1 &
pid1=$!
disown %+
./main >& out.2 &
pid2=$!
disown %+

#
# Run DTrace with different pid probes, each case is its own "epoch":
#                   pid1?      pid2?
#   - 1              no         no
#   - $pid1          YES        no
#   - $pid2          no         YES
#   - *              YES        YES
#

for pid in 1 $pid1 $pid2 '*'; do
	sleep 1
	$dtrace $dt_flags -Zn 'test_prov'$pid':::go { trace("hi"); }
                               tick-1s { exit(0) }'
	if [ $? -ne 0 ]; then
		echo ERROR: dtrace
		kill -TERM $pid1
		kill -TERM $pid2
		exit 1
	fi
	sleep 1

        # Use USR1 to mark epochs in the output.
	kill -USR1 $pid1
	kill -USR1 $pid2
done

echo done
echo "========== out 1"; cat out.1
echo "========== out 2"; cat out.2

echo success

kill -TERM $pid1
kill -TERM $pid2

exit 0
