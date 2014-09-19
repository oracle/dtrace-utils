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
# Copyright 2013, 2014 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.

#
# This script tests that name lookups in the main program, in libraries, of
# interposed functions and in other lmids all works, and resolves the right
# symbol.  It repeats the tests many times, and verifies correctness itself,
# to detect rare races related to SIGSTOP handling.
#

# @@timeout: 3600

export LD_BIND_NOW=t

run()
{
    # TODO: replace with printf -v start '%(%s)T' -1 when we have a newer bash
    start="$(date +%s)"
    # TODO: replace with printf '%(%s)T' -1 when we have a newer bash
    while [[ $(($(date +%s) - start)) -lt 600 ]]; do
        test/triggers/libproc-lookup-by-name $1 $2 test/triggers/libproc-lookup-victim $3 > $tmpdir/lookup.out 2>&1
        if ! grep -q 'Address hit.' $tmpdir/lookup.out || grep -q 'Lookup error.' $tmpdir/lookup.out; then
            cat $tmpdir/lookup.out >&2
            echo "Failure in $1 $2 $3 lookup" >&2
            exit 1
        fi
    done
}

run main_func 0 0
run lib_func 0 1
run interposed_func 0 2
run lib_func 1 3

echo 'All OK.'
