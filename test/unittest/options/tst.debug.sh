#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xdebug -n 'BEGIN { exit(0); }' 2>&1 | \
	gawk '$2 == "DEBUG" && int($3) > 0 {
		cnt[$1]++;
	     }
	     END {
		for (lib in cnt) {
		   printf "%16s %8d debug messages\n", lib, cnt[lib];
		   sum += cnt[lib];
		}
		exit(sum > 0 ? 0 : 1);
	     }'

exit $?
