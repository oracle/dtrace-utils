#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2018, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# If /proc/kallmodsyms does not exist, there is nothing to test

[[ -r /proc/kallmodsyms ]] || exit 2

# The test depends on a kernel fix to report kernel (and built-in-module)
# symbol sizes correctly in /proc/kallmodsyms.  An easy check is to count
# how many kernel and built-in-module symbols (that is, symbols that appear
# before _end or __brk_limit) have zero size.  There should be many (over 1000?),
# often of the form __key.*.  If there are few (2-3), the bug is present
# and this test should not be run.

nzero=`awk '/ 0 /; / _end$/ || / __brk_limit$/ {exit(0);}' /proc/kallmodsyms  | wc -l`

if [[ $nzero -lt 20 ]]; then
	echo "unpatched kernel? /proc/kallmodsyms symbol sizes look suspicious"
	exit 1
fi

exit 0
