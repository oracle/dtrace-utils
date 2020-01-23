#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test is a bit naughty; it's assuming that libc.so has an implementation
# of strup(3), and that it's implemented in terms of the libc.so
# implementation of malloc(3).  If you're reading this comment because
# those assumptions have become false, please accept my apologies...
#
# @@xfail: dtv2

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -qs /dev/stdin -c "/bin/wc -l /dev/zero" <<EOF
pid\$target::strdup:entry
{
	self->strdup = 1;
}

pid\$target:libc.so:malloc:entry
/self->strdup/
{
	ufunc(ucaller);
	exit(0);
}

pid\$target::strdup:return
/self->strdup/
{
	self->strdup = 0;
}
EOF

exit 0
