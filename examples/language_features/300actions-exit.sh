/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/bin/bash

# SYNOPSIS
#   sudo ./300actions-exit.sh
#
# DESCRIPTION
#   The exit() action terminates the script, returning an
#   unsigned 8-bit integer value.

/usr/sbin/dtrace -n '
BEGIN
{
	x = 123;
	y = 456;
	exit(x < y ? x : y);
}'

echo $?
