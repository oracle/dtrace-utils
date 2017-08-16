#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
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
disown %+
sleep 1
if rmmod sdt; then
    ret=1
fi
kill $dt_pid
rmmod dt_test
modprobe sdt >/dev/null 2>/dev/null
modprobe dt_test >/dev/null 2>/dev/null
# Sleep here to give the kernel time to reduce all refcounts before the
# harness tries to unload everything.
sleep 1
exit $ret
