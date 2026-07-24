#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION:
#	The dtrace:::BEGIN trampoline uses the namespace-aware PID helper in
#	a non-initial PID namespace, and keeps the old helper in the initial
#	PID namespace.
#
# @@timeout: 25

. test/unittest/pidns/common.bash

check_dtrace_arg "$@"
dtrace=$1
init_pidns_ino=$((0xEFFFFFFC))
cur_pidns_ino=`stat -Lc %i /proc/self/ns/pid 2>/dev/null`

check_current_namespace()
{
	out=`"$dtrace" $dt_flags -xdisasm=8 -S -n 'BEGIN { exit(0); }' 2>&1`
	status=$?
	if [ $status -ne 0 ]; then
		echo "$out"
		return $status
	fi

echo "$out"
	if ! echo "$out" | grep -q 'call bpf_get_current_pid_tgid'; then
		echo "initial PID namespace did not use bpf_get_current_pid_tgid"
		return 1
	fi
	if echo "$out" | grep -q 'call bpf_get_ns_current_pid_tgid'; then
		echo "initial PID namespace unexpectedly used bpf_get_ns_current_pid_tgid"
		return 1
	fi
}

check_nested_namespace()
{
	out=`run_in_pidns "$dtrace" $dt_flags -xdisasm=8 -S -n 'BEGIN { exit(0); }' 2>&1`
	status=$?
	if [ $status -ne 0 ]; then
		echo "$out"
		return $status
	fi

echo "$out"
	if ! echo "$out" | grep -q 'call bpf_get_ns_current_pid_tgid'; then
		echo "nested PID namespace did not use bpf_get_ns_current_pid_tgid"
		return 1
	fi
	if echo "$out" | grep -q 'call bpf_get_current_pid_tgid'; then
		echo "nested PID namespace unexpectedly used bpf_get_current_pid_tgid"
		return 1
	fi
}

if [ -n "$cur_pidns_ino" ] && [ "$cur_pidns_ino" = "$init_pidns_ino" ]; then
	check_current_namespace || exit $?
fi

check_nested_namespace
exit $?
