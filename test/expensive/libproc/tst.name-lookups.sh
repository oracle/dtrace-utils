#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

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
