#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@xfail: dtv2

##
#
# ASSERTION:
# Testing -lm option with SDT probes in an existing kernel module,
# both with and without wildcarding.
#
# SECTION: dtrace Utility/-lm Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -lm dt_test && $dtrace $dt_flags -lm 'dt_t*'
status=$?

exit $status
