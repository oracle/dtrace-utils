#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies the nusdtprobes option.
# @@timeout: 100

dtrace=$1

# Set up test directory.

DIRNAME=$tmpdir/nusdtprobes.$$.$RANDOM
mkdir -p $DIRNAME
cd $DIRNAME

# Make the trigger.

cat << EOF > prov.d
provider testprov {
	probe foo0();
	probe foo1();
	probe foo2();
	probe foo3();
	probe foo4();
	probe foo5();
	probe foo6();
	probe foo7();
	probe foo8();
	probe foo9();
};
EOF

cat << EOF > main.c
#include <unistd.h>
#include "prov.h"

int
main(int argc, char **argv)
{
	while (1) {
		usleep(1000);

		TESTPROV_FOO0();
		TESTPROV_FOO1();
		TESTPROV_FOO2();
		TESTPROV_FOO3();
		TESTPROV_FOO4();
		TESTPROV_FOO5();
		TESTPROV_FOO6();
		TESTPROV_FOO7();
		TESTPROV_FOO8();
		TESTPROV_FOO9();
	}

	return 0;
}
EOF

# Build the trigger.

$dtrace $dt_flags -h -s prov.d
if [ $? -ne 0 ]; then
	echo "failed to generate header file" >&2
	exit 1
fi
gcc $test_cppflags -c main.c
if [ $? -ne 0 ]; then
	echo "failed to compile test" >&2
	exit 1
fi
$dtrace $dt_flags -G -64 -s prov.d main.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >&2
	exit 1
fi
gcc $test_ldflags -o main main.o prov.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >&2
	exit 1
fi

# Test nusdtprobes settings.
#
# We will start teams of processes, each with 4 members, each in turn
# with 10 USDT probes.  So, regardless of how many teams are run in
# succession, at any one time DTrace needs room for at least 40 USDT
# probes.  The default and -xnusdtprobes=40 settings should work, but
# -xnusdtprobes=39 should not.
nteams=2
nmmbrs=4

for nusdt in "" "-xnusdtprobes=40" "-xnusdtprobes=39"; do

	echo try '"'$nusdt'"'

	# Start DTrace.

	rm -f dtrace.out
	$dtrace $dt_flags $nusdt -Zq -o dtrace.out -n '
	testprov*:::
	{
		@[probeprov, probemod, probefunc, probename] = count();
	}' &
	dtpid=$!
	sleep 2
	if [[ ! -d /proc/$dtpid ]]; then
		echo ERROR dtrace died
		cat dtrace.out
		exit 1
	fi

	# Start teams of processes, only one team at a time.

	rm -f check.txt
	for (( iteam = 0; iteam < $nteams; iteam++ )); do
		# Start the team, writing out expected output.
		sleep 2
		for (( immbr = 0; immbr < $nmmbrs; immbr++ )); do
			./main &
			pids[$immbr]=$!
			disown %+
			for j in `seq 0 9`; do
				echo testprov${pids[$immbr]} main main foo$j >> check.txt
			done
		done

		# Kill the team.
		sleep 3
		for (( immbr = 0; immbr < $nmmbrs; immbr++ )); do
			kill ${pids[$immbr]}
		done
	done

	# Kill DTrace.

	kill $dtpid
	wait

	# Strip the count() value out since we do not know its exact value.

	awk 'NF == 5 { print $1, $2, $3, $4 }' dtrace.out | sort > dtrace.out.sorted

	# Check.

	sort check.txt > check.txt.sorted
	if [ x$nusdt == x"-xnusdtprobes=39" ]; then
		# Results should not agree with check.txt.
		if diff -q check.txt.sorted dtrace.out.sorted; then
			echo ERROR unexpected agreement
			cat dtrace.out
			exit 0
		fi
	else
		# Results should agree with check.txt.
		if ! diff -q check.txt.sorted dtrace.out.sorted; then
			echo ERROR output disagrees
			echo === expected ===
			cat check.txt.sorted
			echo === got ===
			cat dtrace.out.sorted
			echo === diff ===
			diff check.txt.sorted dtrace.out.sorted
			exit 1
		fi
	fi
done

echo success

exit 0
