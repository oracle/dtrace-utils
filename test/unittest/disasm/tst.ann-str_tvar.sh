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
	self->CheckVariable = "abc";
	trace(self->CheckVariable);
}
' 2>&1 | gawk '/ call dt_get_tvar/ { sub(/^[^:]+: /, ""); print; }'

exit $?
