#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xcore -n 'BEGIN { exit(0); }'
file core | tee /dev/stderr | \
	gawk 'BEGIN { rc = 1; }
	     /ELF/ && /core file/ && /dtrace/ { rc = 0; next; }
	     END { exit(rc); }'
rc=$?
rm -f core

exit $rc
