#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# @@xfail: dtrace does not support wildcard pids for pid probes

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

# Note that awk is stripped and new versions (gawk 4.2.1 and higher) probably
# are not built --export-dynamic.  So pid probes might not work well with awk.
# Replace awk with a different trigger.

$dtrace $dt_flags -wZq -x switchrate=100ms -s /dev/stdin <<EOF
pid*:awk::
{
	printf("%s:%s\n", probefunc, probename);
}

tick-1s
/i++ > 5/
{
	exit(0);
}

tick-1s
/(i % 2) == 0/
{
	system("$dtrace $dt_flags -c awk -n 'pid\$target::main:entry{exit(0);}' >/dev/null 2>&1");
}

tick-1s
/(i % 2) == 1/
{
	system("$dtrace $dt_flags -c awk -n 'pid\$target::main:entry{exit(0);}' >/dev/null 2>&1");
}
EOF

exit $?
