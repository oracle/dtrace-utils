#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -qs /dev/stdin << EOT |& grep '^BPF: ' > /dev/null
#pragma D option bpflog

BEGIN
{
	exit(0);
}
EOT

exit $?
