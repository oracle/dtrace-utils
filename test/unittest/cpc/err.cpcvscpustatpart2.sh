#!/bin/bash -p
# 
# Oracle Linux DTrace.
# Copyright (c) 2009, 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# This tests that enablings from the cpc provider will fail if cpustat(1) is
# already master of the universe.
#
# This script will fail if:
#       1) The system under test does not define the 'PAPI_tot_ins'
#       generic event.

# @@skip: no cpustat on Linux, and should switch PAPI_tot_ins to cpu-clock

script()
{
        $dtrace $dt_flags -s /dev/stdin <<EOF
        #pragma D option bufsize=128k

        BEGIN
        {
                exit(0);
        }

        cpc:::PAPI_tot_ins-all-10000
        {
                @[probename] = count();
        }
EOF
}

if [ $# != 1 ]; then
        echo expected one argument: '<'dtrace-path'>'
        exit 2
fi

dtrace=$1
dtraceout=/tmp/dtrace.out.$$

cpustat -c PAPI_tot_ins 1 20 &
pid=$!
sleep 5
script 2>/dev/null

status=$?

kill $pid
exit $status
