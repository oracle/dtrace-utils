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
io:::wait-done
{
	exit(0);
}
' 2>&1 | awk '/ call dt_get_tvar/ { sub(/^[^:]+: /, ""); print; }'

exit $?