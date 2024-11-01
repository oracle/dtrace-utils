#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@nosort

dtrace=$1

$dtrace $dt_flags -qs /dev/stdin << EOF
#pragma D option lockmem=1

BEGIN
{
	exit(0);
}
EOF
