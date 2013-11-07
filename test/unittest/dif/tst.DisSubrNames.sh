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
# Copyright 2013 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
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
