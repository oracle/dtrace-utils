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

# create the directory where the CTF archive will end up
mkdir -p mydirectory/kernel

# sanity check that DTrace works
$dtrace $dt_flags -n 'BEGIN {exit(0)}'
if [ $? -ne 0 ]; then
	echo ERROR: sanity check on DTrace failed
	exit 1
else
	echo sanity check on DTrace passed
fi

# verify that modpath directs us to the (empty) directory
$dtrace $dt_flags -xmodpath=mydirectory -n 'BEGIN {exit(0)}'
if [ $? -ne 0 ]; then
	echo DTrace correctly fails when modpath is an empty directory
else
	echo ERROR: DTrace unexpectedly passes with modpath set to empty directory
	exit 1
fi

# link a CTF archive to our directory
ln -s /lib/modules/$(uname -r)/kernel/vmlinux.ctfa mydirectory/kernel/vmlinux.ctfa

# verify that modpath now works
$dtrace $dt_flags -xmodpath=mydirectory -n 'BEGIN {exit(0)}'
if [ $? -ne 0 ]; then
	echo ERROR: DTrace somehow fails
	exit 1
else
	echo DTrace works with modpath and a linked CTF archive
	exit 0
fi
