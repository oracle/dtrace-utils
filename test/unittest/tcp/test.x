#!/bin/bash
#
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Copyright © 2017, Oracle and/or its affiliates. All rights reserved.
#
#

check_provider tcp
if (( $? != 0 )); then
        echo "Could not load tcp provider"
        exit 1
fi

if grep -qF ../ip/client.ip.pl $_test &&
   ! perl -MIO::Socket::IP -e 'exit(0);' 2>/dev/null; then
	echo "No IO::Socket::IP"
	exit 1
fi
exit 0
