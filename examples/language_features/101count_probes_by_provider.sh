/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/bin/bash

#  SYNOPSIS
#    ./101count_probes_by_provider.sh
#
#  DESCRIPTION
#    The second column lists probes' providers.  Count how
#    many probes there are for each provider.  Use "tail" to
#    start at the second line of output, since the first line
#    has the column headers, which we want to ignore.

sudo /usr/sbin/dtrace -l | tail -n +2 | awk '{print $2}' | sort | uniq -c
