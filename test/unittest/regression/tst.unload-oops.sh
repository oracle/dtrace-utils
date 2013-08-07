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
# Copyright 2013 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#

##
#
# ASSERTION:
# While dtrace is running, DTrace modules cannot be rmmoded, even modules that
# are not outwardly needed for the test in question: a successful removal in
# this situation may oops.
#
##

# We assume that everything is already insmoded at test start, because the
# testsuite driver does it.

ret=0;
set -x
dtrace=$1
$dtrace $dt_flags -n dt_test:::test &
dt_pid=$!
sleep 1
if rmmod sdt >/dev/null 2>&1; then
    ret=1
fi
kill $dt_pid
if ! rmmod dt_test >/dev/null 2>&1; then
   ret=1
fi
modprobe sdt >/dev/null 2>/dev/null
modprobe dt_test >/dev/null 2>/dev/null
exit $ret
