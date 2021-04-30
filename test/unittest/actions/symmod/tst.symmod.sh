#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

# pick a test symbol from /proc/kallmodsyms
read ADD NAM MOD <<< `awk '/ksys_write/ {print $1, $4, $5}' /proc/kallmodsyms`

# a blank module means the module is vmlinux
if [ x$MOD == x ]; then
	MOD=vmlinux
fi

# add the module to the name
NAM=$MOD'`'$NAM

# run DTrace to test mod() and sym()
read MYMOD MYNAM <<< `$dtrace -qn 'BEGIN {mod(0x'$ADD'); sym(0x'$ADD'); exit(0) }'`
if [ $? -ne 0 ]; then
	exit 1
fi

# reporting
echo test $ADD $MOD   $NAM
echo expect    $MOD   $NAM
echo actual  $MYMOD $MYNAM

if [ $MOD != $MYMOD ]; then
	echo fail: $MOD does not match $MYMOD
	exit 1
fi
if [ $NAM != $MYNAM ]; then
	echo fail: $NAM does not match $MYNAM
	exit 1
fi

exit 0
