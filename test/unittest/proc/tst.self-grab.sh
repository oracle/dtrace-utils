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
# Copyright 2016 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.

#
# This script tests that self-grabs work and do not disturb the death-counter
# that causes dtrace -c to exit when its children die (nor fail in any other
# way).
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
exec $dtrace $dt_flags -c 'sleep 2' -s /dev/stdin >/dev/null <<EOF
syscall::write:entry
{
	ustack(1);
}
tick-20s
{
	exit(1);
}
EOF
