#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xverbose -n 'BEGIN { exit(0); }' 2>&1 | \
	awk '{ print; }
	     /^Disassembly of clause :::BEGIN/ { hdr = 1; next; }
	     /^[0-9]{4} [0-9]{5}: [0-9a-f]{2} / { ins++; next; }
	     END {
		exit(hdr && ins > 20 ? 0 : 1);
	     }'

exit $?
