#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# @@xfail: not working yet

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

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

exit $status
