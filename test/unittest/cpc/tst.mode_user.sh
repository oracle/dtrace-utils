#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Test CPC provider with modes (all, user, kernel) with a
# user-space-intensive workload.

# @@reinvoke-failure: 2

utils=`pwd`/test/utils

dtrace=$1
tmpfile=$tmpdir/tst.mode_user.$$
mkdir $tmpfile
cd $tmpfile

target=workload_user

# run tests
declare -A actual
status=0
for eventname in `$utils/cpc_get_events.sh`; do

	# the DTrace CPC provider needs '-' turned into '_'
	Deventname=`echo $eventname | tr '-' '_'`

	# determine number of iterations for target number of seconds
	nsecs=1
	niters=`$utils/workload_get_iterations.sh $target $nsecs`
	if [ $niters -lt 0 ]; then
		echo "workload_get_iterations.sh failed with $target"
		exit 1
	fi

	# determine expected count
	expect=`$utils/perf_count_event.sh $eventname $target $niters`

	# sample events (with DTrace)
	period=$(($expect / 20))
	echo $niters iterations for $Deventname with period $period
	for mode in all user kernel; do
		$dtrace $dt_flags -qn $Deventname-$mode-$period'
		/pid == $target/
		{
			@ = sum('$period');
		}' -c "$utils/$target $niters" > tmp.txt
		if [[ $? -ne 0 ]]; then
			echo ERROR running DTrace for $Deventname-$mode-$period
			cat tmp.txt
			exit 1
		fi
		actual[$mode]=`cat tmp.txt`
		if [ -z ${actual[$mode]} ]; then
			actual[$mode]=0
		fi
	done

	# report
	margin=$(($expect / 3))
	$utils/check_result.sh ${actual[all]}    $expect $margin; status=$(($status + $?))
	$utils/check_result.sh ${actual[user]}   $expect $margin; status=$(($status + $?))
	$utils/check_result.sh ${actual[kernel]}    0    $margin; status=$(($status + $?))
done

exit $status
