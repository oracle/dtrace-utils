#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION: Variables of array type should be aligned based on their element
#            datatype
#
# SECTION: Variables
#
##

dtrace=$1

$dtrace $dt_flags -Sen '
char dummy;
short var[5];

BEGIN
{
	var[3] = 0x1234;
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
		$2 = $6 = "";
		gsub(/ +/, " ");
		printf "%-16s %-6s %-3s %-3s %-4s", $1, $2, $3, $4, $5;
		$1 = $2 = $3 = $4 = $5 = "";
		gsub(/ +/, " ");
		print $0;
		rc = 0;
	}
}

END {
	exit(rc);
}
'

exit $?
