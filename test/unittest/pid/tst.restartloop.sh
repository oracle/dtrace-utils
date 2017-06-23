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
# Copyright 2014, 2017 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#

# @@runtest-opts: -p $_pid
# @@trigger: execloop
# @@trigger-timing: after
# @@skip: until the pid provider is implemented

#
# This script tests that a process execing in a tight loop can still be
# correctly grabbed.
#
script()
{
	# Just quit at once.
	$dtrace -Z $dt_flags -s /dev/stdin <<EOF
	BEGIN
	{
		exit(0);
	}

        pid\$1:libnonexistent.so.1::entry
        {
        }
EOF
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

NUM=0;
while [[ $NUM -lt 5000 ]]; do
	NUM=$((NUM+1))
	if ! script; then
		exit 1
	fi
done
