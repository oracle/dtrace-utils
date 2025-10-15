#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

dtrace=$1

# Check that dtrace runs by default even if ulimit -l is very low.
ulimit -l 1

$dtrace $dt_flags -c test/triggers/delaydie -qn '
syscall::write:entry
/pid == $target/
{
	printf("|%s|", copyinstr(arg1, 32));
}'

exit $?
