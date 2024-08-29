#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

# Test that annotation of gvars is not confused by agg vars.

$dtrace $dt_flags -Sen '
BEGIN
{
	@myvar_agg = count();
	myvar_global = 0xdeadbeef;
	exit(0);
}
' 2>&1 | grep -A4 deadbeef | gawk '/myvar_/ {print $NF}'

exit $?
