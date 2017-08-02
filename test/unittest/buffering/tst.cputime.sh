#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2006, 2011, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@timeout: 12

script()
{
	$dtrace $dt_flags -s /dev/stdin -x bufpolicy=$1 $1 <<EOF

	#pragma D option quiet
	#pragma D option statusrate=1hz

	uint64_t total;
	int thresh;

	BEGIN
	{
		start = timestamp;
		thresh = 10;
	}

	sched:::on-cpu
	/pid == \$pid/
	{
		self->on = vtimestamp;
	}

	sched:::off-cpu
	/self->on/
	{
		total += vtimestamp - self->on;
	}

	tick-1sec
	/i++ == 10/
	{
		exit(0);
	}

	END
	/((total * 100) / (timestamp - start)) > thresh/
	{
		printf("'%s' buffering policy took %d%% of CPU; ",
		    \$\$1, ((total * 100) / (timestamp - start)));
		printf("expected no more than %d%%!\n", thresh);
		exit(1);
	}
EOF
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

for policy in "fill ring switch"; do
	script $policy

	status=$?

	if [ "$status" -ne 0 ]; then
		exit $status
	fi
done

exit 0
