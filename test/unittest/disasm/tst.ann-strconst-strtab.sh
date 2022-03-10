#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# The process of linking BPF functions together into a program caused the
# strtab for a DIFO to not contain all the strings needed for the code.  This
# was evidenced through incorrect string constant annotations on the linked and
# final program disassembler listings.
#
# This test verifies that this problem is no longer present.
#

dtrace=$1

$dtrace $dt_flags -xdisasm=12 -Sn '
BEGIN
{
	a["a"] = 42;
	exit(0);
}
' 2>&1 | \
	awk '/^Disassembly of/ {
		kind = $3;
		next;
	     }
	     / ! "/ {
		sub(/^[^:]+:/, kind);
		sub(/ 07 [0-9] /, " 07 X ");
		sub(/[0-9a-f]{8}    add/, "XXXXXXXX    add");
		sub(/%r[0-9], [0-9]+ +/, "%rX, D ");
		print;
	     }'

exit $?
