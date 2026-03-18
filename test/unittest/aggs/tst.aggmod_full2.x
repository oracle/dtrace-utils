#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2018, 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Let us skip this test altogether:
#   * The test was never vetted on arm.
#   * The test does not run on newer kernels since they have no kallmodsyms.
#   * On older x86 platforms, the test has failed consistently for a long time.
# The test had been intended for checking /proc/kallmodsyms,
# but we are migrating to a modules.builtin.ranges approach.
# When we are absolutely sure we do not care about this test, it can be removed.

echo "test is deprecated"
exit 2

###############################################
### The older contents of this file follow. ###
###############################################

# The test still needs to be vetted on ARM64, including for copyinstr() support.
# Once it runs on ARM64, the slower tst.aggmod_full.sh test can be removed.

if [[ `uname -m` = "aarch64" ]]; then
	echo "test has not get been vetted on ARM64"
	exit 2
fi

# If /proc/kallmodsyms does not exist, there is nothing to test.

[[ -r /proc/kallmodsyms ]] || exit 2

# The test depends on a kernel fix to report kernel (and built-in-module)
# symbol sizes correctly in /proc/kallmodsyms.  An easy check is to count
# how many kernel and built-in-module symbols (that is, symbols that appear
# before _end or __brk_limit) have zero size.  There should be many (over 1000?),
# often of the form __key.*.  If there are few (2-3), the bug is present
# and this test should not be run.

nzero=`gawk '/ 0 /; / _end$/ || / __brk_limit$/ {exit(0);}' /proc/kallmodsyms  | wc -l`

if [[ $nzero -lt 20 ]]; then
	echo "unpatched kernel? /proc/kallmodsyms symbol sizes look suspicious"
	exit 1
fi

exit 0
