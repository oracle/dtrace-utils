#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# The -p option errors when used to grab a nonexistent process.
#
# SECTION: dtrace Utility/-p Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

# This is slightly racy: that's unavoidable.
# Picking really high PIDs by default should help.

pid="$(($RANDOM + 10240000))"
while kill -0 $pid 2>/dev/null; do
    pid="$(($RANDOM + 10240000))"
done

$dtrace $dt_flags -q -p $pid -P proc
