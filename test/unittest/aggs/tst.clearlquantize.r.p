#!/bin/awk -f
# Oracle Linux DTrace.
# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
# Check that all the graphs printed have at least one nonzero element.

BEGIN {
    fail = 0;
    seenat = 0;
    active = 0;
}

/value /,/^$/ { active = 1; }

/^$/ {
    if (active && !seenat) {
        printf("Unexpectedly empty result seen.\n");
        fail = 1;
    }

    active = 0;
}

/@/ {
    if (active)
        seenat = 1;
}

END {
    if (!fail)
        printf "All populated as expected.\n";
}
