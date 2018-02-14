#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@xfail: inconsistent results

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

file=/tmp/out.$$
dtrace=$1

rm -f $file

$dtrace $dt_flags -o $file -c date -s /dev/stdin <<EOF

	#pragma D option quiet
	#pragma D option bufsize=1M
	#pragma D option bufpolicy=fill

	pid\$target:::entry,
	pid\$target:::return,
	pid\$target:a.out::,
	syscall:::return,
	profile:::profile-997
	/pid == \$target/
	{
        	printf("START %s:%s:%s:%s\n",
            	probeprov, probemod, probefunc, probename);
        	trace(ustackdepth);
        	ustack(100);
        	trace("END\n");
	}

	tick-1sec
	/n++ == 10/
	{
		trace("test timed out...");
		exit(1);
	}
EOF

status=$?
if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
	rm -f $file
	exit $status
fi

perl /dev/stdin $file <<EOF
	while (<>) {
		chomp;

		last if /^\$/;

		die "expected START at \$.\n" unless /^START/;

		\$_ = <>;
		chomp;
		die "expected depth (\$_) at \$.\n" unless /^(\d+)\$/;
		\$depth = \$1;

		for (\$i = 0; \$i < \$depth; \$i++) {
			\$_ = <>;
			chomp;
			die "unexpected END at \$.\n" if /^END/;
		}

		\$_ = <>;
		chomp;
		die "expected END at \$.\n" unless /^END\$/;
	}
EOF

status=$?

count=`wc -l $file | cut -f1 -do`
if [ "$count" -lt 1000 ]; then
	echo $tst: output was too short
	status=1
fi

rm -f $file

exit $status
