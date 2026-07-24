#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION:
#	Concurrent DTrace consumers in one non-initial PID namespace only
#	match their own BEGIN firing.
#
# @@timeout: 25

. test/unittest/pidns/common.bash

check_dtrace_arg "$@"
dtrace=$1
DIRNAME="$tmpdir/pidns-concurrent.$$.$RANDOM"
mkdir -p "$DIRNAME"

run_in_pidns bash -s "$dtrace" "$DIRNAME" <<'EOS'
dtrace=$1
dir=$2

"$dtrace" $dt_flags -qn 'BEGIN { printf("A\n"); exit(0); }' \
    > "$dir/a.out" 2> "$dir/a.err" &
p1=$!

"$dtrace" $dt_flags -qn 'BEGIN { printf("B\n"); exit(0); }' \
    > "$dir/b.out" 2> "$dir/b.err" &
p2=$!

wait $p1
s1=$?
wait $p2
s2=$?

if [ $s1 -ne 0 ] || [ $s2 -ne 0 ]; then
	cat "$dir/a.out" "$dir/a.err" "$dir/b.out" "$dir/b.err"
	exit 1
fi

if [ "`cat "$dir/a.out"`" != "A" ]; then
	echo "consumer A produced unexpected output:"
	cat "$dir/a.out" "$dir/a.err"
	exit 1
fi

if [ "`cat "$dir/b.out"`" != "B" ]; then
	echo "consumer B produced unexpected output:"
	cat "$dir/b.out" "$dir/b.err"
	exit 1
fi

exit 0
EOS

status=$?
rm -rf "$DIRNAME"
exit $status
