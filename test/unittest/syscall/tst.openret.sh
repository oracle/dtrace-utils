#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

script() {
	$dtrace $dt_flags -c 'cat shajirosan' -qs /dev/stdin <<EOF
	syscall::open*:entry
	/pid == \$target/
	{
		self->p = arg0;
	}

	syscall::open*:return
	/self->p && copyinstr(self->p) == "shajirosan"/
	{
		self->err = 1;
		self->p = 0;
	}

	syscall::open*:return
	/self->err && (int)arg0 == -2 && (int)arg1 == -2/
	{
		exit(0);
	}

	syscall::open*:return
	/self->err/
	{
		printf("a failed open(2) returned %d\n", (int)arg0);
		exit(1);
	}

	syscall::open*:return
	/self->p/
	{
		self->p = 0;
	}
EOF
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

script
status=$?

exit $status
