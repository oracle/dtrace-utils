#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test is a bit naughty; it's assuming that ld.so.1 has an implementation
# of calloc(3C), and that it's implemented in terms of the ld.so.1
# implementation of malloc(3C).  If you're reading this comment because
# those assumptions have become false, please accept my apologies...
#

# @@xfail: pid provider not yet implemented, needs porting

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -qs /dev/stdin -c "/bin/echo" <<EOF
pid\$target:ld.so.1:calloc:entry
{
	self->calloc = 1;
}

pid\$target:ld.so.1:malloc:entry
/self->calloc/
{
	@[umod(ucaller), ufunc(ucaller)] = count();
}

pid\$target:ld.so.1:calloc:return
/self->calloc/
{
	self->calloc = 0;
}

END
{
	printa("%A %A\n", @);
}
EOF

exit 0
