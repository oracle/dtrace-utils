#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This script tests that monitoring a multithreaded process that exec()s
# in a thread other than the main thread does not hang.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
exec $dtrace $dt_flags -c test/triggers/proc-tst-pthread-exec -n 'BEGIN'
