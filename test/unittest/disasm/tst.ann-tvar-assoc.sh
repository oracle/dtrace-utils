#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -Sen '
self int st[int], ld[int];

BEGIN
{
	self->st[2] = 42;
	trace(self->ld[5]);
	exit(0);
}
' 2>&1 | gawk '/ call dt_get_assoc/ { sub(/^[^:]+: /, ""); print; }'

exit $?
