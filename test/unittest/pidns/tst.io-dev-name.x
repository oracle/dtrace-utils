#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

if [ "$UID" -ne 0 ]; then
	echo "loopback filesystem test requires root"
	exit 2
fi

for cmd in dd mkfs.xfs mount umount losetup; do
	command -v "$cmd" >/dev/null 2>&1 || {
		echo "$cmd not available"
		exit 2
	}
done

exit 0
