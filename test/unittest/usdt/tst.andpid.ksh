#
# Oracle Linux DTrace.
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -c date -s /dev/stdin <<EOF
plockstat\$target::mutex_lock_impl:,
pid\$target::mutex_lock_impl:
{}
EOF

if [ $? -ne 0 ]; then
	print -u2 "dtrace failed"
	exit 1
fi

exit 0
