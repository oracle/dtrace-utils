#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# @@nosort

dtrace=$1

# once n becomes too small, there are insufficient registers
for n in 8 7 6 5 4 3; do

    $dtrace $dt_flags -xiregs=$n -qn 'BEGIN {
        a = b = c = d = e = f = g = h = i = j = k = l = 1;
        trace(a + b * (c + d * (e + f * (g + h * (i + j * (k + l))))));
        exit(0);
    }'

    echo "iregs $n gives $?"
done
