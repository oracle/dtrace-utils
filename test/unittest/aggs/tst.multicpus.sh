#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

DIRNAME="$tmpdir/aggs_multicpus.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

#
# Run a D script that fires on every CPU,
# forcing DTrace to aggregate results over all CPUs.
#

$dtrace $dt_flags -qn '
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
    BEGIN {
        xcnt = xavg = xstm = xstd = xsum = 0;
        xmin = +1000000000;
        xmax = -1000000000;
    }

    # the first "cpu" lines provide the inputs to the aggregations
    /^cpu [0-9]*$/ {

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

    # the remaining lines are the aggregation results
    {
        # first we finish computing our estimates for avg and stddev
        # (the other results require no further action)

        xavg /= xcnt;

        xstm /= xcnt;
        xstd /= xcnt;
        xstd -= xstm * xstm;
        xstd = int(sqrt(xstd));

        # now read the results and compare

        getline; if ($1 != xcnt) { printf("ERROR: cnt, expect %d got %d\n", xcnt, $1) };
        getline; if ($1 != xavg) { printf("ERROR: avg, expect %d got %d\n", xavg, $1) };
        getline; if ($1 != xstd) { printf("ERROR: std, expect %d got %d\n", xstd, $1) };
        getline; if ($1 != xmin) { printf("ERROR: min, expect %d got %d\n", xmin, $1) };
        getline; if ($1 != xmax) { printf("ERROR: max, expect %d got %d\n", xmax, $1) };
        getline; if ($1 != xsum) { printf("ERROR: sum, expect %d got %d\n", xsum, $1) };
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
