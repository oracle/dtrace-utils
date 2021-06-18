#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1
out=$tmpdir/output.$$

watch_output_for() {
    for iter in `seq 10`; do
        sleep 1
        if grep -q $1 $out; then
            iter=0
            break
        fi
    done
    if [[ $iter -ne 0 ]]; then
        echo ERROR: missing $1 in output file
        exit 1
    fi
}

$dtrace $dt_flags -n BEGIN,END -o $out &
watch_output_for :BEGIN
kill %1
watch_output_for :END
echo success
