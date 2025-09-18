#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# We should not be able to trace on a hlt instruction, which is forbidden
# by the kernel.
#
# (Test tst.entry_off0.sh should be okay, since there the probe on that
# instruction is optional, and dtrace will silently skip over such
# problematic instructions.  Here, however, the probe on the instruction
# is explicitly requested by the user.)

dtrace=$1
trig=`pwd`/test/triggers/ustack-tst-basic
off=`${OBJDUMP} -d $trig | awk '/hlt/ {sub(/:/, ""); print $1}'`

$dtrace $dt_flags -c $trig -n 'pid$target:a.out:-:'$off',pid$target:a.out::
{
	trace("hlt instruction");
	exit(0);
}'
