#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -Sen '
BEGIN
{
	this->CheckVariable = "abc";
	trace(this->CheckVariable);
}
' 2>&1 | awk '/this->CheckV/ {
		sub(/^[^:]+: /, "");          # strip line number

		gsub(/%r[0-9]/, "%rX");       # hide reg numbers
		$2 = "N";
		$3 = "N";

		print;                        # print
	}'

exit $?
