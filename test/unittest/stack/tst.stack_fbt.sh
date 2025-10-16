#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Check the stack action for expected frames.

dtrace=$1

# Set up test directory.

DIRNAME=$tmpdir/stack_fbt.$$.$RANDOM
mkdir -p $DIRNAME
cd $DIRNAME

# Use DTrace to capture stack() at vfs_write:entry.

$dtrace $dt_flags -wqn '
BEGIN
{
	system("echo write something > /dev/null");
}

fbt::vfs_write:entry
{
	stack();
	exit(0);
}' >& dtrace.out

if [ $? -ne 0 ]; then
	echo ERROR: dtrace failed
	cat dtrace.out
	exit 1
fi

# Ignore blank lines and strip out
# - ".constprop.[0-9]"
# - "_after_hwframe"    (x86 starting with UEK8)
# - "+0x[0-9a-f]*$"
# - leading spaces

awk 'NF != 0 { sub("\\.constprop\\.[0-9]", "");
               sub("_after_hwframe\\+", "+");
               sub(/+0x[0-9a-f]*$/, "");
               sub(/^ */, "");
               print }' dtrace.out > dtrace.post
if [ $? -ne 0 ]; then
	echo ERROR: awk failed
	cat dtrace.out
	exit 1
fi

# Identify, in order, a few frames we expect to see.

if [ $(uname -m) == "x86_64" ]; then
	frames="vfs_write do_syscall_64 entry_SYSCALL_64"
elif [ $(uname -m) == "aarch64" ]; then
	frames="vfs_write __arm64_sys_write el0_svc_common el0_svc"
else
	echo ERROR: unrecognized platform
	uname -r
	uname -m
	exit 1
fi
for frame in $frames; do
	echo 'vmlinux`'$frame >> dtrace.cmp
done

# Compare results.

diff dtrace.cmp dtrace.post | grep '^<' > missing.frames
if [ `cat missing.frames | wc -l` -ne 0 ]; then
	echo ERROR: missing some expected frames
	echo === expected frames include:
	cat dtrace.cmp
	echo === actual frames are:
	cat dtrace.out
	echo === missing expected frames:
	cat missing.frames
	exit 1
fi

echo success
exit 0
