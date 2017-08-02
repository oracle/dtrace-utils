#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This script tests that tracing a system daemon noninvasively correctly
# correctly where possible.  (The process chosen has some regular files open
# under /var, which checks the trickiest code path in Psystem_daemon().)
#
orig_DEBUG="$DTRACE_DEBUG"

script()
{
	rsyslogd_pid="$(cat /var/run/syslogd.pid)"
	DTRACE_DEBUG=t $dtrace $dt_flags -qws /dev/stdin <<EOF
		BEGIN { system("echo foo | logger") }
		syscall::write:entry / pid == $rsyslogd_pid / { ustack(500); }
                tick-1s { i++; }
                tick-1s / i > 4 / { exit(0); }
EOF
}

result_eval()
{
    while read -r line; do
	if [[ "$line" =~ ^[a-z]+' 'DEBUG' '[0-9]+:' ' ]]; then
	    if [[ "$line" =~ "is a system daemon process" ]]; then
		found_sd=t
	    fi
	    if [[ -n $orig_DEBUG ]]; then
		printf "%s\n" "$line" >&2
	    fi
	else
	    if [[ "$line" =~ libpthread.*'`'start_thread ]]; then
                found_start_thread=t
	    fi
	    if [[ "$line" =~ rsyslogd'`'wtiWorker ]]; then
                found_gfn=t
	    fi
	    printf "%s\n" "$line"
	fi
    done

    if [[ -z $found_sd ]]; then
        echo "rsyslog is not properly recognized as a system daemon."
        return 1
    fi

    if [[ -z $found_start_thread ]] || [[ -z $found_gfn ]]; then
        echo "Cannot find both expected symbols. Noninvasive symbol resolution is broken."
        return 1
    fi

    return 0
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

found_sd=
found_start_thread=
found_gfn=

set -o pipefail
script |& result_eval
status=$?

exit $status
