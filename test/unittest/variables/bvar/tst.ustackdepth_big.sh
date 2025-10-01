#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
# ASSERTION: ustackdepth works for larger than default perf_event_max_stack.
#
# SECTION: Variables/Built-in Variables/ustackdepth
##

dtrace=$1

POSTPROC=$PWD/test/unittest/variables/bvar/check_stackdepth_to_stack.awk
TRIGGER=$PWD/test/triggers/ustack-tst-bigstack-spin

DIRNAME=$tmpdir/ustackdepth_big.$$.$RANDOM
mkdir -p $DIRNAME
cd $DIRNAME

orig_maxstack=`sysctl -n kernel.perf_event_max_stack`
echo kernel.perf_event_max_stack was $orig_maxstack
trap "sysctl kernel.perf_event_max_stack=$orig_maxstack" QUIT EXIT
sysctl kernel.perf_event_max_stack=200

$dtrace $dt_flags -c $TRIGGER -qn '
profile-1
/pid == $target/
{
    printf("DEPTH %d\n", ustackdepth);
    printf("TRACE BEGIN\n");
    ustack(200);
    printf("TRACE END\n");
    exit(0);
}
ERROR
{
    exit(1);
}
' > D.out
if [ $? -ne 0 ]; then
    echo DTrace failure
    exit 1
fi

sleep 2
sysctl kernel.perf_event_max_stack=$orig_maxstack

$POSTPROC D.out > awk.out
if [ $? -ne 0 ]; then
    echo post processing failure
    exit 1
fi

if echo "Stack depth OK" | diff -q - awk.out; then
    mydepth=`gawk '/DEPTH/ { print $2 }' D.out`
    if [ $mydepth -gt $orig_maxstack ]; then
        echo success depth $mydepth exceeded original limit $orig_maxstack
        exit 0
    else
        echo ERROR: $mydepth does not exceed original limit $orig_maxstack
        cat D.out
        exit 1
    fi
else
    echo "ERROR: stack depth does not match stack"
    cat D.out
    exit 1
fi

exit 0
