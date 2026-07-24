#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

command -v unshare >/dev/null 2>&1 || {
	echo "unshare not available"
	exit 2
}

command -v timeout >/dev/null 2>&1 || {
	echo "timeout not available"
	exit 2
}

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`
if [ "$MAJOR" -lt 5 ] || { [ "$MAJOR" -eq 5 ] && [ "$MINOR" -lt 7 ]; }; then
	echo "bpf_get_ns_current_pid_tgid unavailable before Linux 5.7"
	exit 2
fi

if ! timeout --signal=TERM --kill-after=5 5 unshare -fp -- true >/dev/null 2>&1; then
	echo "cannot create PID namespace"
	exit 2
fi

exit 0
