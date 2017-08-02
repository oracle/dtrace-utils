#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# Using SDT, then removing and reinserting it and doing a dtrace -l should not
# panic the kernel.
#
##

# We assume that everything is already insmoded at test start, because the
# testsuite driver does it.

dtrace=$1

$dtrace $dt_flags -qn proc:::exit -n 'tick-1s { exit(0); }'
rmmod sdt
modprobe sdt >/dev/null 2>/dev/null
$dtrace $dt_flags -l > /dev/null

exit 0
