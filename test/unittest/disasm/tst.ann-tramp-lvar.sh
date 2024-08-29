#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xdisasm=2 -Sn '
BEGIN,
io:::start
{
	exit(0);
}
' 2>&1 | gawk '/this->/ {
		sub(/^[^:]+: /, "");

		gsub(/%r[0-9]/, "%rX");
		$2 = "N";
		$3 = "N";

		print;
         }'

exit $?
