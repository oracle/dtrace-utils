#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

DIRNAME="$tmpdir/builtinvar-id_ERROR.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# Have a D script report the probe ID within an ERROR probe.

$dtrace $dt_flags -qn '
BEGIN { *((int*)0) }
BEGIN { exit(1) }
ERROR { printf("ERROR probe id is %d\n", id); exit(0); }
' -o D.out 2> D.err
if [ $? -ne 0 ]; then
    echo DTrace failed
    echo ==== D.out
    cat D.out
    echo ==== D.err
    cat D.err
    exit 1
fi

# Get the ERROR probe ID from "dtrace -l" output.

id=`$dtrace $dt_flags -ln dtrace:::ERROR |& gawk '/^ *[0-9]* *dtrace *ERROR *$/ { print $1 }'`

# Construct expected output.

echo "ERROR probe id is $id" > D.out.chk
echo >> D.out.chk

# Check output.

if ! diff -q D.out D.out.chk; then
    echo output mismatches
    echo ==== D.out
    cat D.out
    echo ==== D.out.chk
    cat D.out.chk
    exit 1
fi

echo success
exit 0
