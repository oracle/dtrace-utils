#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@timeout: 100

dtrace=$1

DIRNAME="$tmpdir/misc-BPFlog-pragma.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# form a D script that:
# - fails the BPF verifier (due to its high complexity)
# - requires a larger-than-default-size BPF log file

cat > D.d << EOF
/* #pragma D option bpflogsize=nnnn */
BEGIN
{
	x = 1;
EOF

for x in `seq 2000`; do
	echo "	@ = quantize(x);" >> D.d
done

cat >> D.d << EOF
	exit(0);
}
EOF

# form the expected output when the BPF log size is too small
# ("nnnn" represents the size that appears in the actual output)

cat > D.out << EOF
BPF verifier log is incomplete and is not reported.
Set DTrace option 'bpflogsize' to some greater size for more output.
(Current size is nnnn.)
dtrace: could not enable tracing: BPF program load for 'dtrace:::BEGIN' failed: Bad address
EOF

# dump files
function dump_files() {
	# the output (just the head and tail if it is huge)
	echo "========== output"
	if [ `cat tmp.out | wc -l` -gt 30 ]; then
		head -10 tmp.out
		echo "..."
		tail -10 tmp.out
	else
		cat tmp.out
	fi

	# the D script (which has a large number of duplicate lines)
	echo "========== uniq -d D.d"
	uniq -c D.d
	echo "=========="
}

# try the script with increasing BPF log size (starting with default)

cursiz=$((16 * 1024 * 1024 - 1))     # default size from dt_bpf_load_prog()
while [ $cursiz -lt $((1024 * 1024 * 1024)) ]; do

	# dtrace should not pass
	$dtrace $dt_flags -qs D.d >& tmp.out
	if [ $? -eq 0 ]; then
		echo unexpected pass
		dump_files
		exit 1
	fi

	# Usually (at least in the default case),
	# dtrace should fail because the BPF log size is too small.
	# Output should match D.out.
	if [ `sed s/$cursiz/nnnn/ tmp.out | diff - D.out | wc -l` -eq 0 ]; then
		echo okay: $cursiz is too small

		# bump the size up and try again
		cursiz=$((2 * $cursiz + 1))
		sed -i 's:^.*bpflogsize.*$:#pragma D option bpflogsize='$cursiz':' D.d
		continue
	elif grep -q '^\/\* #pragma D option bpflogsize=nnnn \*\/$' D.d; then
		echo "The default case is expected to have bpflogsize too small"
		echo "and fail with the expected error."
		echo "========== output (expected)"
		sed 's/nnnn/'$cursiz'/' D.out
		dump_files
		exit 1
	fi

	# determine the actual size of the BPF log
	# (It's the "BPF:" tagged output, minus the "BPF:" prefix on each line.)
	actsiz=`grep "^BPF: " tmp.out | cut -c6- | wc -c`

	# confirm that we failed with a huge dump
	status=0
	if [ $actsiz -lt $(($cursiz / 2)) ]; then
		echo ERROR: BPF output should have fit in smaller buffer
		status=1
	fi
	if [ $actsiz -gt $cursiz ]; then
		echo ERROR: BPF output is larger than user-specified limit: $actsiz '>' $cursiz
		status=1
	fi
	if [ `grep -cv '^BPF: ' tmp.out` -ne 1 ]; then
		echo ERROR: expected only one non-\"BPF:\" output line
		status=1
	fi
	if ! grep -q '^BPF: The sequence of 8193 jumps is too complex.' tmp.out; then
		echo ERROR: BPF error message is missing
		status=1
	fi
	if ! grep -q '^dtrace: could not enable tracing: BPF program load for '\''dtrace:::BEGIN'\'' failed: Bad address' tmp.out; then
		echo ERROR: dtrace error message is missing
		status=1
	fi
	if [ $status -ne 0 ]; then
		dump_files
	else
		echo SUCCESS: test failed with expected error
	fi
	exit $status
done

echo "ERROR: BPF log size, currently $cursiz, has gotten unexpectedly large"
exit 1
