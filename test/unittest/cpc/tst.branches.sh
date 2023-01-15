#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# @@reinvoke-failure: 1

utils=`pwd`/test/utils

dtrace=$1
DIRNAME=$tmpdir/tst.branches.$$
mkdir $DIRNAME
cd $DIRNAME

# determine number of iterations for target number of seconds
nsecs=1
niters=`$utils/workload_get_iterations.sh workload_user $nsecs`
if [ $niters -lt 0 ]; then
	echo "workload_get_iterations.sh failed with workload_user"
	exit 1
fi

# pick a sampling period
period=$(($niters / 100))

# run DTrace
$dtrace $dt_flags -qn '
BEGIN
{
	n = 0;
}

branches-all-'$period'
/pid == $target/
{
	n++;
}

END
{
	printf("%d\n", n);
}' -c "$utils/workload_user $niters" -o tmp.txt

if [[ $? -ne 0 ]]; then
	echo ERROR running DTrace
	cat tmp.txt
	exit 1
fi

# estimate actual count (sampling period * # of samples)
actual=$(($period * `cat tmp.txt`))

# determine expected count (one branch per interation)
expect=$niters

# check
$utils/check_result.sh $actual $expect $(($expect / 4))
exit $?
