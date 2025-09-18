#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# Although the D script takes only "one second," it takes a long time to
# shut down.  Until that has been solved, increase the timeout for the test.
# @@timeout: 240

dtrace=$1

trig=`pwd`/test/triggers/ustack-tst-basic

DIRNAME="$tmpdir/enter_off0.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# Run DTrace, dumping all probe functions and names, plus PC, in a.out.

$dtrace $dt_flags -c $trig -n '
pid$target:a.out::
{
	@[probefunc, probename, uregs[R_PC]] = count();
}

profile:::tick-1s
{
	exit(0);
}' > D.out
if [ $? -ne 0 ]; then
	echo ERROR: dtrace
	cat D.out
	exit 1
fi

# Generate the expected list of functions for our trigger program.

echo main > expected.tmp
echo mycallee >> expected.tmp
for x in 0 1 2 3 4 5 6 7 8 9 \
         a b c d e f g h i j k l m n o p q r s t u v w x y z \
         A B C D E F G H I J K L M N O P Q R S T U V W X Y Z; do
    echo myfunc_$x >> expected.tmp
done
sort expected.tmp > expected.txt

# Check output for probe name "0" or "entry".

awk '$2 == "0" || $2 == "entry"' D.out | awk '
{
	fun = $1;
	prb = $2;
	PC = $3;
	cnt = $4;
}

# Check that the count is 1.
cnt != 1 {
	print "ERROR: count is not 1";
	print;
	exit(1);
}

# Check that we have not gotten the same (fun,prb) already.
prb == "0" && fun in PC0 {
	print "ERROR: already have offset 0 for this func";
	print;
	exit(1);
}
prb == "entry" && fun in PCentry {
	print "ERROR: already have entry for this func";
	print;
	exit(1);
}

# Record the PC.
prb ==   "0"   { PC0[fun] = PC; }
prb == "entry" { PCentry[fun] = PC; }

# Do final matchup.
END {
	# Walk functions for the offset-0 probes.
	for (fun in PC0) {
		# Make sure each offset-0 probe has a matching entry probe.
		if (!(fun in PCentry)) {
			print "ERROR: func", fun, "has offset-0 but no entry";
			exit(1);
		}

		# Make sure the matching probes report the same PC.
		if (PC0[fun] != PCentry[fun]) {
			print "ERROR: func", fun, "has mismatching PCs for offset-0 and entry:", PC0[fun], PCentry[fun];
			exit(1);
		}

		# Dump the function name and delete these entries.
		print fun;
		delete PC0[fun];
		delete PCentry[fun];
	}

	# Check if there are any leftover entry probes.
	for (fun in PCentry) {
		print "ERROR: func", fun, "has entry but no offset-0";
		exit(1);
	}
}
' | sort > awk.out

# Report any problems.

if ! diff -q awk.out expected.txt; then
	echo ERROR: diff failure
	echo ==== function list
	cat awk.out
	echo ==== expected function list
	cat expected.txt
	echo ==== diff
	diff awk.out expected.txt
	echo ==== DTrace output
	cat D.out
	exit 1
fi

echo success
exit 0
