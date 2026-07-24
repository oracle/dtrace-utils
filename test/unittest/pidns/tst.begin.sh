#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION:
#	BEGIN fires when DTrace runs in a non-initial PID namespace.
#
# @@timeout: 20

. test/unittest/pidns/common.bash

check_dtrace_arg "$@"
dtrace=$1

out=`run_in_pidns "$dtrace" $dt_flags -qn 'BEGIN { printf("begin\n"); exit(0); }' 2>&1`
status=$?

if [ $status -ne 0 ]; then
	echo "$out"
	exit $status
fi

if [ "$out" != "begin" ]; then
	echo "expected BEGIN output, got:"
	echo "$out"
	exit 1
fi

exit 0
