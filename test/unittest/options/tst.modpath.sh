#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

DIRNAME="$tmpdir/modpath.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# Create the directory where the CTF archive will end up.
mkdir -p mydirectory/kernel

# Basic check that DTrace works.
$dtrace $dt_flags                       -lvP rawtp > basic.out
if [ $? -ne 0 ]; then
	echo ERROR: basic check on DTrace failed
	cat basic.out
	exit 1
else
	echo basic check on DTrace passed
fi

# Verify that modpath directs us to the (empty) directory, with no CTF info.
$dtrace $dt_flags -xmodpath=mydirectory -lvP rawtp > empty.out
if [ $? -ne 0 ]; then
	echo ERROR: empty check on DTrace failed
	cat empty.out
	exit 1
else
	echo empty check on DTrace passed
fi

# Link a CTF archive to our directory.
ln -s /lib/modules/$(uname -r)/kernel/vmlinux.ctfa mydirectory/kernel/vmlinux.ctfa

# Verify that modpath now works.
$dtrace $dt_flags -xmodpath=mydirectory -lvP rawtp > final.out
if [ $? -ne 0 ]; then
	echo ERROR: final check on DTrace failed
	cat final.out
	exit 1
else
	echo final check on DTrace passed
fi

# Check the amount of CTF info we got from each run by seeing how many
# rawtp args have non-trivial type info.  While nempty should be 0,
# nbasic and nfinal should be substantial and likely identical (but
# we allow a generous tolerance).

nbasic=`awk '/^[	 ]*args\[/ && !/uint64_t/ && !/void \*/' basic.out | wc -l`
nempty=`awk '/^[	 ]*args\[/ && !/uint64_t/ && !/void \*/' empty.out | wc -l`
nfinal=`awk '/^[	 ]*args\[/ && !/uint64_t/ && !/void \*/' final.out | wc -l`

if [ $nempty -ne 0 ]; then
	echo ERROR: empty check turned up some CTF info
	cat empty.out
	echo got nbasic $nbasic nempty $nempty nfinal $nfinal
	exit 1
fi
if [ $nbasic -lt 1000 ]; then
	echo ERROR: basic check turned up little CTF info
	cat basic.out
	echo got nbasic $nbasic nempty $nempty nfinal $nfinal
	exit 1
fi
if [ $nfinal -lt 1000 ]; then
	echo ERROR: final check turned up little CTF info
	cat final.out
	echo got nbasic $nbasic nempty $nempty nfinal $nfinal
	exit 1
fi
if [ $nbasic -gt $(($nfinal + 200)) -o $nfinal -gt $((nbasic + 200)) ]; then
	echo $nbasic $nfinal out of range
	echo ==== basic.out:
	cat basic.out
	echo ==== final.out:
	cat final.out
	echo got nbasic $nbasic nempty $nempty nfinal $nfinal
	exit 1
fi

echo all checks pass
exit 0
