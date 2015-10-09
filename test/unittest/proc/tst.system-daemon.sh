#!/bin/bash
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2015 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.

# @@xfail: noninvasive symbol resolution is broken

#
# This script tests that tracing a system daemon noninvasively correctly
# identifies the process as a system daemon, and resolves addresses in it
# correctly where possible.  (The process chosen has some regular files open
# under /var, which checks the trickiest code path in Psystem_daemon().)
#
orig_DEBUG="$DTRACE_DEBUG"

script()
{
	rsyslogd_pid="$(cat /var/run/syslogd.pid)"
	DTRACE_DEBUG=t $dtrace $dt_flags -qws /dev/stdin <<EOF
		BEGIN { system("echo foo | logger") }
		syscall::write:entry / pid == $rsyslogd_pid / { ustack(); }
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
	    if [[ "$line" =~ 'libpthread.so.0`start_thread' ]]; then
                found_start_thread=t
	    fi
	    if [[ "$line" =~ 'rsyslogd`genFileName' ]]; then
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
