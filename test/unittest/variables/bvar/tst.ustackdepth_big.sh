#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
# ASSERTION: ustackdepth works for larger than default perf_event_max_stack.
#
# SECTION: Variables/Built-in Variables/ustackdepth
##

dtrace=$1

orig_maxstack=`sysctl -n kernel.perf_event_max_stack`
echo kernel.perf_event_max_stack was $orig_maxstack
sysctl kernel.perf_event_max_stack=200

$dtrace $dt_flags -c test/triggers/ustack-tst-bigstack-spin -qn '
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
'
if [ $? -ne 0 ]; then
    echo DTrace failure
    exit 1
fi

sleep 2
sysctl kernel.perf_event_max_stack=$orig_maxstack

exit 0
