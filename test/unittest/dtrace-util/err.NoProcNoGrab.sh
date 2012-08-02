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
# The -p option errors when used to grab a nonexistent process.
#
# SECTION: dtrace Utility/-p Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

# This is slightly racy: that's unavoidable.
# Picking really high PIDs by default should help.

pid="$(($RANDOM + 10240000))"
while kill -0 $pid 2>/dev/null; do
    pid="$(($RANDOM + 10240000))"
done

$dtrace $dt_flags -q -p $pid -P proc
