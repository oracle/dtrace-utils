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
# Copyright 2012 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#

##
#
# ASSERTION:
# The -c option can be used to invoke a new process, and catches both the
# process invocation and the process termination event when it exits.
#
# NOTE: This can be improved when proc:::exec-success reports the execed
# filename.
#
# SECTION: dtrace Utility/-c Option
#
##

# @@timeout: 10

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -q -c '/bin/true' -n '
proc:::exec-success /pid == $target/ { printf("exec of %s seen\n", execname); } 
proc:::exit /pid == $target/ { printf ("exit seen, exitcode %i\n", arg0); }'

status=$?

if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
fi

exit $status
