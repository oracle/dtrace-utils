#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Test the stack action with default stack depth and depth 3.

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
	printf("first 3 frames\n");
	stack(3);
	exit(0);
}' >& dtrace.out

if [ $? -ne 0 ]; then
	echo ERROR: dtrace failed
	cat dtrace.out
	exit 1
fi

# Strip out
# - blank lines
# - "constprop"
# - "isra"
# - "_after_hwframe"    (x86 starting with UEK8)
# - pointer values

awk 'NF != 0 { sub("\\.constprop\\.[0-9]", "");
               sub("\\.isra\\.[0-9]", "");
               sub("_after_hwframe\\+", "+");
               sub(/+0x[0-9a-f]*$/, "+{ptr}");
               print }' dtrace.out > dtrace.post
if [ $? -ne 0 ]; then
	echo ERROR: awk failed
	cat dtrace.out
	exit 1
fi

# Figure out what stack to expect.

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -eq 5 -a $MINOR -lt 8 ]; then
	# up to 5.8
	KERVER="A"
else
	# starting at 5.8
	KERVER="B"
fi

if [ $(uname -m) == "x86_64" -a $KERVER == "A" ]; then
cat << EOF > dtrace.cmp
              vmlinux\`vfs_write+{ptr}
              vmlinux\`__x64_sys_write+{ptr}
              vmlinux\`x64_sys_call+{ptr}
              vmlinux\`do_syscall_64+{ptr}
              vmlinux\`entry_SYSCALL_64+{ptr}
EOF
elif [ $(uname -m) == "aarch64" -a $KERVER == "A" ]; then
cat << EOF > dtrace.cmp
              vmlinux\`vfs_write
              vmlinux\`__arm64_sys_write+{ptr}
              vmlinux\`el0_svc_common+{ptr}
              vmlinux\`el0_svc_handler+{ptr}
              vmlinux\`el0_svc+{ptr}
EOF
elif [ $(uname -m) == "x86_64" -a $KERVER == "B" ]; then
cat << EOF > dtrace.cmp
              vmlinux\`vfs_write+{ptr}
              vmlinux\`ksys_write+{ptr}
              vmlinux\`do_syscall_64+{ptr}
              vmlinux\`entry_SYSCALL_64+{ptr}
EOF
elif [ $(uname -m) == "aarch64" -a $KERVER == "B" ]; then
cat << EOF > dtrace.cmp
              vmlinux\`vfs_write
              vmlinux\`__arm64_sys_write+{ptr}
              vmlinux\`invoke_syscall+{ptr}
              vmlinux\`el0_svc_common+{ptr}
              vmlinux\`do_el0_svc+{ptr}
              vmlinux\`el0_svc+{ptr}
              vmlinux\`el0t_64_sync_handler+{ptr}
              vmlinux\`el0t_64_sync+{ptr}
EOF
else
	echo ERROR: unrecognized platform
	uname -r
	uname -m
	exit 1
fi

# Add the first 3 frames a second time.

head -3 dtrace.cmp > dtrace.tmp
echo first 3 frames >> dtrace.cmp
cat dtrace.tmp >> dtrace.cmp

# Compare results.

if ! diff -q dtrace.cmp dtrace.post; then
	echo ERROR: results do not match
	diff dtrace.cmp dtrace.post
	echo "==== expect"
	cat dtrace.cmp
	echo "==== actual"
	cat dtrace.out
	exit 1
fi

echo success

exit 0
