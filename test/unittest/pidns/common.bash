#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

check_dtrace_arg()
{
	if [ $# != 1 ]; then
		echo expected one argument: '<'dtrace-path'>'
		exit 2
	fi
}

run_in_pidns()
{
	timeout --signal=TERM --kill-after=5 "${PIDNS_TEST_TIMEOUT:-15}" \
	    unshare -fp --mount-proc -- "$@"
}
