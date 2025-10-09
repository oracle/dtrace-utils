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
trap "sleep 2; sysctl kernel.perf_event_max_stack=$orig_maxstack" QUIT EXIT

# set bounds on the full stack depth (which is ambiguous)
lo=188
hi=192

function do_dtrace() {
    # set the kernel parameter
    sysctl kernel.perf_event_max_stack=$stack_limit

    # run dtrace
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
    ' > D.out.$stack_limit
    if [ $? -ne 0 ]; then
        echo ERROR: DTrace failure with $stack_limit
        exit 1
    fi

    # check stackdepth consistency
    $POSTPROC D.out.$stack_limit > awk.out.$stack_limit
    if [ $? -ne 0 ]; then
        echo ERROR: post processing failure
        exit 1
    fi
    if ! grep -q "Stack depth OK" awk.out.$stack_limit; then
        echo ERROR: stack depth does not match stack
        cat D.out
        exit 1
    fi

    # get actual stack depth
    mydepth=`gawk '/DEPTH/ { print $2 }' D.out.$stack_limit`
    echo with limit $stack_limit got stack depth $mydepth

    # provide breathing room between dtrace and resetting kernel parameter
    sleep 2
}

# try a stack limit that is too small
stack_limit=$(($lo / 2))
do_dtrace
echo "   " stack limit $stack_limit is too small for the entire stack
if [ $mydepth -ne $stack_limit ]; then
    echo ERROR: $mydepth does not match $stack_limit
    exit 1
fi
echo "   " success:  actual depth matches limit

# try a stack limit that is large enough
stack_limit=$(($hi + 20))
do_dtrace
echo "   " stack limit $stack_limit is large enough
if [ $mydepth -lt $lo ]; then
    echo ERROR: $mydepth is lower than low bound $lo
    exit 1
fi
if [ $mydepth -gt $hi ]; then
    echo ERROR: $mydepth is greater than high bound $hi
    exit 1
fi
echo "   " success:  actual depth captures full stack

# restore the kernel parameter
sysctl kernel.perf_event_max_stack=$orig_maxstack

exit 0
