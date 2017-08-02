#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
##
#
# ASSERTION:
# We do not load CTF unnecessarily for trivial DTrace programs.
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

# These should both load only vmlinux and ctf (the first due to
# dlibs, the second due to both that and direct referencing),
# or perhaps vmlinux, ctf and dtrace.

tiny()
{
    DTRACE_DEBUG=t $dtrace $dt_flags -s /dev/stdin <<EOF
#pragma D option quiet

int64_t foo;

BEGIN {
	foo = 4;
	exit(0);
}

tick-1s {
	trace("foo");
}

EOF
}

bigger()
{
    DTRACE_DEBUG=t $dtrace $dt_flags -s /dev/stdin <<EOF
#pragma D option quiet

proc::create {
	this->pid = ((struct task_struct *)arg0)->pid;
	printf("%s\n", curthread->comm);
	exit(0);
}

tick-1s {
	trace("foo");
}

EOF
}

COUNT1="$(tiny 2>&1 | grep 'loaded CTF' | wc -l)"
COUNT2="$(bigger 2>&1 | grep 'loaded CTF' | wc -l)"

case $COUNT1:$COUNT2 in
 [23]:[23]) exit 0;;
 *) echo "Excessive type loads incurred."
    echo "Tiny:"
    tiny 2>&1 | grep 'loaded CTF'
    echo "Bigger:"
    bigger 2>&1 | grep 'loaded CTF'
    exit 1;;
esac
