#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xcppargs='-H -dM' -Cs /dev/stdin << EOT 2>&1 | \
	awk '/^\.+/ && /\.h$/ { cnt++; }
	     /invalid control directive: #define/ { cnt = -cnt; }
	     { print; }
	     END { exit(cnt < 0 ? 0 : 1); }'
#include <linux/posix_types.h>

BEGIN
{
	exit(0);
}
EOT

exit $?
