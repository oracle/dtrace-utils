#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2016, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

exec $dtrace $dt_flags -c "find /dev/dtrace -exec /bin/true ;" \
    -n 'perf:::sched_process_fork { trace(args[0]->pid); trace (args[1]->pid); hitany = 1; }'
    -n 'END / hitany == 0 / { exit(1); }'
