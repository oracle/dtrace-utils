#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

if ! perf list hw | grep -qw instructions; then
	echo 'no "instructions" event on this system'
	exit 2
fi

exit 0
