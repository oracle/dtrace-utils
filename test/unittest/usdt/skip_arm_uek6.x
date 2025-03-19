#!/bin/bash
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@skip: not run directly by test harness
#
# Tests that depend on USDT argument translation fail on ARM for UEK6.
# They're fine for UEK7.  It is unclear in exactly which kernel they
# start working.

if [[ `uname -m` != "aarch64" ]]; then
	exit 0
fi

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -gt 5 ]; then
	exit 0
fi
if [ $MAJOR -eq 5 -a $MINOR -ge 10 ]; then
	exit 0
fi

echo "USDT argmap not working on ARM on older kernels"
exit 1
