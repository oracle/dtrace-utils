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
	CheckVariable[1234] = "abc";
	trace(CheckVariable[1234]);
}
' 2>&1 | gawk '/ call dt_get_assoc/ { sub(/^[^:]+: /, ""); print; }'

exit $?
