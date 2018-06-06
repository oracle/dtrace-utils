#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# @@tags: unstable

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

file=$tmpdir/out.$$
dtrace=$1

rm -f $file

$dtrace $dt_flags -o $file -c test/triggers/ustack-tst-spin -s /dev/stdin <<EOF

	#pragma D option quiet
	#pragma D option destructive
	#pragma D option evaltime=main

	/*
	 * Toss out the first 100 samples to wait for the program to enter
	 * its steady state.
	 */

	profile-1999
	/pid == \$target && n++ > 100/
	{
		@total = count();
		@stacks[ustack(4)] = count();
	}

	tick-1s
	{
		secs++;
	}

	tick-1s
	/secs > 5/
	{
		done = 1;
	}

	tick-1s
	/secs > 10/
	{
		trace("test timed out");
		exit(1);
	}

	profile-1999
	/pid == \$target && done/
	{
		raise(SIGINT);
		exit(0);
	}

	END
	{
		printa("TOTAL %@u\n", @total);
		printa("START%kEND\n", @stacks);
	}
EOF

status=$?
if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
	rm -f $file
	exit $status
fi

perl /dev/stdin $file <<EOF
	\$_ = <>;
	chomp;
	die "output problem\n" unless /^TOTAL (\d+)/;
	\$count = \$1;
	die "too few samples (\$count)\n" unless \$count >= 1000;

	while (<>) {
		chomp;

		last if /^$/;

		die "expected START at \$.\n" unless /^START/;


		\$_ = <>;
		chomp;
		die "expected END at \$.\n" unless /\`baz\+?/;

		\$_ = <>;
		chomp;
		die "expected END at \$.\n" unless /\`bar\+?/;

		\$_ = <>;
		chomp;
		die "expected END at \$.\n" unless /\`foo\+?/;

		\$_ = <>;
		chomp;
		die "expected END at \$.\n" unless /\`main\+?/;

		\$_ = <>;
		chomp;
		die "expected END at \$.\n" unless /^END\$/;
	}

EOF

status=$?
rm -f $file

exit $status
