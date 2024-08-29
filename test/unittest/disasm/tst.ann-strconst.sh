#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -Sen '
BEGIN
{
	var = "strconst";
	exit(0);
}
' 2>&1 | \
	gawk '/ ! "strconst"/ {
		sub(/^[^:]+: /, "");
		sub(/^07 [0-9] /, "07 X ");
		sub(/[0-9a-f]{8}    add/, "XXXXXXXX    add");
		sub(/%r[0-9], [0-9]+ +/, "%rX, D ");
		print;
	     }'

exit $?
