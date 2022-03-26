#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xdisasm=8 -Sn '
BEGIN, syscall::write:return
{
	exit(0);
}
' 2>&1 | \
	awk '/js[a-z]+/ {
		sub(/^[^:]+: /, "");
		sub(/ +!.*$/, "");
		sub(/ [0-9a-f]{4} /, " XXXX ");
		print;
	     }'

exit $?
