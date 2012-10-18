#!/bin/bash

#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2012 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
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
