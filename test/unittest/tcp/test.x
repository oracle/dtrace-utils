#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2017, 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

if grep -qF ../ip/client.ip.pl $_test &&
   ! perl -MIO::Socket::IP -e 'exit(0);' 2>/dev/null; then
	echo "No IO::Socket::IP"
	exit 1
fi
exit 0
