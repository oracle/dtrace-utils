#!/bin/awk -f
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Copyright © 2016, Oracle and/or its affiliates. All rights reserved.
#
# Check that the sum of all 'before clear' values is nonzero,
# the sum of all 'after clear' values is zero, and the sum of
# all 'final' values are nonzero again.

BEGIN {
    inafter = 0;
    sum = 0;
    fail = 0;
}

$2 ~ /^[0-9]+$/ { sum += $2; }

function checksums(zerop) {
    if (zerop && sum != 0) {
        printf "FAIL: sum %i is nonzero.\n", sum;
        fail = 1;
    }
    else if (!zerop && sum == 0) {
        fail = 1;
        printf "FAIL: sum %i is zero.\n", sum;
    }
}

/after clear:/ {
    checksums(inafter);
    inafter = 1;
    sum = 0;
}

/Final / {
    checksums(inafter);
    inafter = 0;
    sum = 0;
}

END {
    checksums(inafter);

    if (!fail)
        printf "All sums as expected.\n";
}
