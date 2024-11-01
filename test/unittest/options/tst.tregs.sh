#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

for x in 3 5 9 11; do
    $dtrace $dt_flags -xtregs=$x -qn 'BEGIN { @[1, 2, 3, 4, 5, 6, 7] = count(); exit(0) }'
    echo "tregs $x gives $?"
done
