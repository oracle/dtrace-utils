#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION:
#	proc:::create works in a non-initial PID namespace and can translate
#	args[0]->pr_ppid.  On BTF kernels with type tags this exercises
#	tag-transparent task_struct member resolution.
#
# @@timeout: 25

. test/unittest/pidns/common.bash

check_dtrace_arg "$@"
dtrace=$1

run_in_pidns bash -s "$dtrace" <<'EOS'
dtrace=$1
status=1

sleeper()
{
	while true; do
		sleep 1
	done
}

cleanup()
{
	[ -n "$child" ] && kill "$child" 2>/dev/null
}
trap cleanup EXIT

sleeper &
child=$!

"$dtrace" $dt_flags -s /dev/stdin <<EOF
proc:::create
/args[0]->pr_ppid != 0/
{
	exit(0);
}

profile:::tick-12s
{
	exit(1);
}
EOF
status=$?

exit $status
EOS

exit $?
