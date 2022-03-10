#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xdisasm=8 -Sn '
BEGIN
{
	a = b = 0;
	trace(a / b);
	exit(0);
}

ERROR
{
	exit(1);
}
' 2>&1 | \
	awk -vDTRACEFLT_DIVZERO=4 \
	    'BEGIN {
		rc = 1;
	     }
	     /mov  %r3, [0-9]+/ {
		flt = int($10);
		next;
	     }
	     /call dt_probe_error/ && flt == DTRACEFLT_DIVZERO {
		loci[int($1)] = flt;
		next;
	     }
	     /divide-by-zero/ && (int($NF) in loci) {
		rc = 0;
	     }
	     END {
		exit(rc);
	     }'

exit $?
