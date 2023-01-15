#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

actual=$1
expect=$2
margin=$3

printf " check %10s;  against %10s;  margin %10s:  " $actual $expect $margin

if [ $actual -lt $(($expect - $margin)) ]; then
	echo ERROR too small
	exit 1
fi
if [ $actual -gt $(($expect + $margin)) ]; then
	echo ERROR too large
	exit 1
fi

echo success
exit 0
