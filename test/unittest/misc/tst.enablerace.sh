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
# Copyright 2006 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"%Z%%M%	%I%	%E% SMI"

#
# This script attempts to tease out a race when probes are initially enabled.
#
script()
{
	#
	# Nauseatingly, the #defines below must be in the 0th column to
	# satisfy the ancient cpp that -C defaults to.
	#
	$dtrace $dt_flags -C -s test/unittest/misc/tst.enablerace.dpp
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
let i=0

while [ "$i" -lt 20 ]; do
	script
	status=$?

	if [ "$status" -ne 0 ]; then
		exit $status
	fi

	let i=i+1
done

exit 0
