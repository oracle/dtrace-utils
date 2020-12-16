#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@timeout: 120

dtrace=$1

DIRNAME="$tmpdir/misc-BPFlog-pragma.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# form a D script that will not run with defaults and will require a large BPF log file

cat > D.d << EOF
#pragma D option quiet
/* #pragma D option bpflogsize=nnnn */
BEGIN
{
	x = 1;
EOF

for x in `seq 100`; do
	echo "	@ = quantize(x); @ = quantize(x); @ = quantize(x); @ = quantize(x);" >> D.d
done

cat >> D.d << EOF
	exit(0);
}
EOF

# form the expected output when the BPF log size is too small
# ("nn" represents the size that appears in the actual output)

cat > D.out << EOF
BPF verifier log is incomplete and is not reported.
Set DTrace option 'bpflogsize' to some greater size for more output.
(Current size is nn.)
dtrace: could not enable tracing: BPF program load for 'dtrace:::BEGIN' failed: Argument list too long
EOF

# try the script with increasing BPF log size (starting with default)

startedtoosmall=0
cursiz=$((16 * 1024 * 1024 - 1))
while [ $cursiz -lt $((2 * 1024 * 1024 * 1024)) ]; do

	# dtrace should not pass
	$dtrace -s D.d >& tmp.out
	if [ $? -eq 0 ]; then
		echo unexpected pass
		cat D.d
		cat tmp.out
		exit 1
	fi

	# usually, it will fail because the BPF log size is too small
	if [ `sed s/$cursiz/nn/ tmp.out | diff - D.out | wc -l` -eq 0 ]; then
		echo okay: $cursiz is too small

		# confirm that we started too small
		startedtoosmall=1

		# so bump the size up and try again
		cursiz=$((2 * $cursiz + 1))
		sed -i 's:^.*bpflogsize.*$:#pragma D option bpflogsize='$cursiz':' D.d
		continue
	fi

	# confirm that we started too small
	if [ $startedtoosmall -eq 0 ]; then
		echo the smallest sizes we try should be too small
		exit 1
	fi

	# otherwise, it should fail with a huge dump
	actsiz=`cat tmp.out | wc -c`
	status=0
	if [ $actsiz -lt $(($cursiz / 2)) ]; then
		echo ERROR: BPF output should have fit in smaller buffer
		status=1
	fi
	if [ $actsiz -gt $cursiz ]; then
		echo ERROR: BPF output is larger than user-specified limit
		status=1
	fi
	if [ `grep -cv '^BPF: ' tmp.out` -ne 1 ]; then
		echo ERROR: expected only one non-\"BPF:\" output line
		status=1
	fi
	if ! grep -q '^BPF: BPF program is too large. Processed .* insn' tmp.out; then
		echo ERROR: BPF error message is missing
		status=1
	fi
	if ! grep -q '^dtrace: could not enable tracing: BPF program load for .* failed: Argument list too long' tmp.out; then
		echo ERROR: dtrace error message is missing
		status=1
	fi
	if [ $status -ne 0 ]; then
		head -10 tmp.out
		echo "..."
		tail -10 tmp.out
	else
		echo SUCCESS: test failed with expected error
	fi
	exit $status
done

echo "ERROR: BPF log size, currently $cursiz, has gotten unexpectedly large"
exit 1
