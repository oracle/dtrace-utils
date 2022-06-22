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
	myvar_global = 0xdeadbeef;
	@myvar_agg = quantize(myvar_global);
	@myvar_agg_3keys[1, 2, "hi"] = sum(myvar_global);
	@myvar_agg_min = min(myvar_global);
	exit(0);
}
' 2>&1 | awk '/ call dt_get_agg/ { sub(/^[^:]+: /, ""); print; }'

exit $?
