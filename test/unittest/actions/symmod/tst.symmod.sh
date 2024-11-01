#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

# pick a test symbol from /proc/kallsyms
read ADD NAM MOD <<< `gawk '/ ksys_write/ {print $1, $3, $4}' /proc/kallsyms`

# a blank module means the module is vmlinux
if [ x$MOD == x ]; then
	MOD=vmlinux
fi

# add the module to the name
NAM=$MOD'`'$NAM

# run DTrace to test mod() and sym()
# also test func(), but it is simply an alias for sym()
read MYMOD MYNAM MYFUN <<< `$dtrace $dt_flags -qn 'BEGIN {mod(0x'$ADD'); sym(0x'$ADD'); func(0x'$ADD'); exit(0) }'`
if [ $? -ne 0 ]; then
	exit 1
fi

# reporting
echo test $ADD $MOD   $NAM
echo expect    $MOD   $NAM   $NAM
echo actual  $MYMOD $MYNAM $MYFUN

if [ $MOD != $MYMOD ]; then
	echo fail: $MOD does not match $MYMOD
	exit 1
fi
if [ $NAM != $MYNAM ]; then
	echo fail: $NAM does not match $MYNAM
	exit 1
fi
if [ $NAM != $MYFUN ]; then
	echo fail: $NAM does not match $MYFUN
	exit 1
fi

exit 0
