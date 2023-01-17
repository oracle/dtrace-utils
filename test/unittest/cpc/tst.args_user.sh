#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# @@reinvoke-failure: 1

utils=`pwd`/test/utils

dtrace=$1
tmpfile=$tmpdir/tst.args_user.$$
mkdir $tmpfile
cd $tmpfile

target=workload_user

# dump the loop PCs
$utils/workload_analyze_loop.sh $target | awk 'NF == 1' > PCs.txt
echo PCs in the loop: `cat PCs.txt`

# determine number of iterations for target number of seconds
nsecs=2
niters=`$utils/workload_get_iterations.sh $target $nsecs`
if [ $niters -lt 0 ]; then
	echo "workload_get_iterations.sh failed with $target"
	exit 1
fi

# use "perf" to get total
total=`$utils/perf_count_event.sh cpu-clock $target $niters`
if [ $total -lt 0 ]; then
	echo "perf_count_event.sh failed with $target"
	exit 1
fi

# set how many probe firings to expect
expect=100
period=$(($total / $expect))

# run DTrace
echo $niters iterations and period $period
$dtrace $dt_flags -qn '
	cpc:::cpu_clock-all-'$period'
	/pid == $target/
	{
		printf("%x %x\n", arg0, arg1);
	}' -c "$utils/$target $niters" | awk 'NF == 2' | sort | uniq -c > D.out
if [[ $? -ne 0 ]]; then
	echo ERROR running DTrace
	cat D.out
	exit 1
fi

echo "summary of D output (occurrences, arg0, arg1)"
cat D.out

# check the PCs
read ntotal narg0 narg1 nwarn nerror <<< `awk '
BEGIN { ntotal = narg0 = narg1 = nwarn = nerror = 0; }

# one file has the PCs that are in the inner loop
FILENAME == "PCs.txt" { PCs[$1] = 1; next; }

# other file reports 1:occurrences, 2:arg0, 3:arg1
                                             { ntotal += $1 }
($2 in PCs)                                  { narg0  += $1 }
($3 in PCs)                                  { narg1  += $1 }
$2 != 0                                      { nwarn  += $1 }
$2 == 0 && $3 == 0                           { nerror += $1 }
$2 != 0 && $3 != 0                           { nerror += $1 }
rshift(strtonum("0x"$2), 63) == 0 && $2 != 0 { nerror += $1 }
rshift(strtonum("0x"$3), 63) != 0            { nerror += $1 }

# report
END {
	print ntotal, narg0, narg1, nwarn, nerror;
}' PCs.txt D.out`

# report
status=0
margin=$(($expect / 10))
$utils/check_result.sh $ntotal $expect $margin; status=$(($status + $?))
$utils/check_result.sh $narg0     0    $margin; status=$(($status + $?))
$utils/check_result.sh $narg1  $expect $margin; status=$(($status + $?))
$utils/check_result.sh $nwarn     0    $margin; status=$(($status + $?))
$utils/check_result.sh $nerror    0       0   ; status=$(($status + $?))

exit $status
