#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# ASSERTION:
#
# That dtrace -S knows the names of (at least some) DTrace subrs.
# This guards against silent errors building dt_names.c.
#
# We test rand() and d_path() because they are the lowest- and highest-
# numbered D subrs at the present time.

dtrace=$1

$dtrace $dt_flags -Seq -n 'BEGIN { d_path(0); rand(); }' 2>&1 | tee $tmpdir/dis-subr-names.out

if ! grep -q '! rand' $tmpdir/dis-subr-names.out ||
   ! grep -q '! d_path' $tmpdir/dis-subr-names.out; then
    echo 'dtrace -S does not know of d_path() or rand().' >&2
    exit 1
fi

if grep -q '! unknown' $tmpdir/dis-subr-names.out; then
    echo 'dtrace -S mentions unknown subrs.' >&2
    exit 1
fi
