#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

if ! check_provider udp; then
        echo "Could not load udp provider"
        exit 1
fi
if ! perl -MIO::Socket::IP -e 'exit(0);' 2>/dev/null; then
	exit 1
fi
exit 0
