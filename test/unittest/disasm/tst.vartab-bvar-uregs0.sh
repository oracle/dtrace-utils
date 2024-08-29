#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION: Built-in variables should not displayed with an offset
#	     Test uregs[0] separately until it works on all kernel versions.
#
# SECTION: Variables/Scalar Variables
#
##

dtrace=$1

$dtrace $dt_flags -Sen '
sdt:task::task_rename
{
	trace(uregs[0]);
	exit(0);
}
' 2>&1 | gawk '
BEGIN {
	rc = 1;
}

/^NAME/ && /KND SCP/ {
	printf "%-16s %-6s %-3s %-3s %-4s %s\n",
	       "NAME", "OFFSET", "KND", "SCP","FLAG", "TYPE";
	while (getline == 1 && NF > 0) {
		if ($3 == "scl" || $3 == "arr") {
			$2 = $5 = "";
			gsub(/ +/, " ");
			printf "%-16s %-6s %-3s %-3s %-4s", $1, "", $2, $3, $4;
			$1 = $2 = $3 = $4 = "";
			gsub(/ +/, " ");
		} else {
			$2 = $6 = "";
			gsub(/ +/, " ");
			printf "%-16s %-6s %-3s %-3s %-4s", $1, $2, $3, $4, $5;
			$1 = $2 = $3 = $4 = $5 = "";
			gsub(/ +/, " ");
		}

		print $0;
		rc = 0;
	}
}

END {
	exit(rc);
}
'

exit $?
