#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

DIRNAME="$tmpdir/libdir.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# place a faulty D program in a subdirectory
mkdir subdirectory
cat << EOF > subdirectory/faulty_D_program.d
BEGIN {
	nonexistent_action();
}
EOF

# check that the subdirectory (and its faulty program) is found
$dtrace $dt_flags -xlibdir=subdirectory -n 'BEGIN {exit(0)}'
if [ $? -eq 0 ]; then
	echo ERROR: DTrace should have failed due to faulty D program
	exit 1
else
	echo DTrace found the faulty D program in the specified subdirectory
	exit 0
fi
