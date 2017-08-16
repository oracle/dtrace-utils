#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# The -c option can be used to invoke a new process, and catches both the
# process invocation and the process termination event when it exits, but
# does not catch process invocation if evaltime is not 'exec'.
#
# NOTE: This can be improved when proc:::exec-success reports the execed
# filename.
#
# SECTION: dtrace Utility/-c Option
#
##

# @@timeout: 10

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

for name in exec preinit postinit main; do 
    $dtrace $dt_flags -q -c '/bin/true' -x evaltime=$name -s /dev/stdin <<EOF
proc:::exec-success /pid == \$target/ { printf("exec of %s under $name seen\n", execname); } 
proc:::exit /pid == \$target/ { printf ("exit seen, reason %i\n", args[0]); }
EOF
done

$dtrace $dt_flags -q -c '/bin/true' -s /dev/stdin <<EOF
proc:::exec-success /pid == \$target/ { printf("exec of %s under default seen\n", execname); } 
proc:::exit /pid == \$target/ { printf ("exit seen, reason %i\n", args[0]); }
EOF
