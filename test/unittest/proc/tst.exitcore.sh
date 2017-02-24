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
# Copyright 2007 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"

#
# This script tests that the proc:::exit probe fires with the correct argument
# when the process core dumps.
#
# If this fails, the script will run indefinitely; it relies on the harness
# to time it out.
#

# Coredump to names that we can distinguish from each other: don't
# suppress coredumps.

orig_core_pattern="$(cat /proc/sys/kernel/core_pattern)"
echo core.%e > /proc/sys/kernel/core_pattern
orig_core_uses_pid="$(cat /proc/sys/kernel/core_uses_pid)"
echo 0 > /proc/sys/kernel/core_uses_pid
ulimit -c unlimited

script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	proc:::exit
	/curpsinfo->pr_ppid == $child &&
	    curpsinfo->pr_psargs == "$longsleep" && args[0] == CLD_DUMPED/
	{
		exit(0);
	}

	proc:::exit
	/curpsinfo->pr_ppid == $child &&
	    curpsinfo->pr_psargs == "$longsleep" && args[0] != CLD_DUMPED/
	{
		printf("Child process could not dump core.");
		exit(1);
	}
EOF
}

sleeper()
{
	cd $tmpdir
	ulimit -c 4096
	while true; do
		$longsleep &
		disown %+
		sleep 1
		kill -SEGV $!
	done
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
longsleep="sleep 10000"

sleeper &
child=$!
disown %+

script
status=$?

pkill -P $child
kill $child

echo $orig_core_pattern > /proc/sys/kernel/core_pattern
echo $orig_core_uses_pid > /proc/sys/kernel/core_uses_pid

exit $status
