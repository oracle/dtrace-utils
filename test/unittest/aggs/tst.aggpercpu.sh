#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

DIRNAME="$tmpdir/aggpercpu.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

#
# Run a D script that fires on every CPU,
# forcing DTrace to aggregate results over all CPUs.
#

$dtrace  $dt_flags -xaggpercpu -qn '
    profile-600ms
    {
        printf("cpu %d\n", cpu);
        @xcnt = count();
        @xavg = avg(10 * cpu + 3);
        @xstd = stddev(20 * cpu + 8);
        @xmin = min(30 * cpu - 10);
        @xmax = max(40 * cpu - 15);
        @xsum = sum(50 * cpu);
    }
    tick-900ms
    {
        exit(0)
    }
' > dtrace.out
if [ $? -ne 0 ]; then
    echo DTrace failed
    cat dtrace.out
    exit 1
fi

#
# Examine the results.
#

gawk '
    # The expected value for the aggregation is aggval.
    # The expected value on a CPU is (m * cpu + b).
    # The default value on a CPU that did not fire is defval.
    function check(label, aggval, m, b, defval) {
        # Check the aggregation over all CPUs.
        getline;
        print "check:", $0;
        if ($1 != aggval) { printf("ERROR: %s, expect %d got %d\n", label, aggval, $1) };

        # Check the per-CPU values.
        for (i = 1; i <= ncpu; i++) {
            do {
                getline;
                print "check:", $0;

                # pass is +1 for pass, -1 for error, 0 for skip CPU
                pass = 0;

                if (match($0, "^    \\[CPU ") != 1) {
                    print "ERROR: no CPU to read"
                    pass = -1;
                } else if (strtonum($2) > cpu[i]) {
                    print "ERROR: skipped over expected CPU"
                    pass = -1;
                } else if (strtonum($2) == cpu[i]) {
                    if (strtonum($3) != m * cpu[i] + b) {
                        print "ERROR: wrong value"
                        pass = -1;
                    } else {
                        # right value
                        pass = +1;
                    }
                } else if ($3 != defval) {
                    print "ERROR: wrong default value"
                    pass = -1;
                } else {
                    # skip over CPU that apparently did not fire
                    pass = 0;
                }

                if (pass == -1)
                    printf("ERROR: %s, agg per cpu %d, line: %s\n", label, cpu[i], $0);
            } while (pass == 0);
        }
    }

    BEGIN {
        xcnt = xavg = xstm = xstd = xsum = 0;
        xmin = +1000000000;
        xmax = -1000000000;
        ncpu = 0;
    }

    # The first "cpu" lines provide the inputs to the aggregations.
    /^cpu [0-9]*$/ {
	cpu[++ncpu] = strtonum($NF);

        xcnt += 1;

        x = 10 * $2 + 3;
        xavg += x;

        x = 20 * $2 + 8;
        xstm += x;
        xstd += x * x;

        x = 30 * $2 - 10;
        if (xmin > x) { xmin = x };

        x = 40 * $2 - 15;
        if (xmax < x) { xmax = x };

        x = 50 * $2;
        xsum += x;

        next;
    }

    # The remaining lines are the aggregation results.
    {
        # First we finish computing our estimates for avg and stddev.
        # (The other results require no further action.)
        # (We keep truncating to ints to mimic DTrace algorithms.)

        xavg /= xcnt;             xavg = int(xavg);

        xstm /= xcnt;             xstm = int(xstm);
        xstd /= xcnt;             xstd = int(xstd);
        xstd -= xstm * xstm;
        xstd = sqrt(xstd);        xstd = int(xstd);

        # Sort the cpus.

        asort(cpu);

        # Now read the results and compare.

        check("cnt", xcnt,  0,   1, "0");
        check("avg", xavg, 10,   3, "0");
        check("std", xstd,  0,   0, "0");
        check("min", xmin, 30, -10,  "9223372036854775807");
        check("max", xmax, 40, -15, "-9223372036854775808");
        check("sum", xsum, 50,   0, "0");

        printf("done\n");
    }
' dtrace.out > awk.out
if [ $? -ne 0 ]; then
    echo awk failed
    cat dtrace.out
    exit 1
fi

if grep -q ERROR awk.out ; then
    echo ERROR found
    echo "=================================================="
    cat dtrace.out
    echo "=================================================="
    cat awk.out
    echo "=================================================="
    exit 1
fi

echo success
exit 0
