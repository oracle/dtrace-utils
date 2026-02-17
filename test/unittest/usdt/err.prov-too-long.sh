#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# Ensure that provider names longer than 53 chars are rejected at link time.

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi


dtrace=$1

DIRNAME="$tmpdir/prov-too-long.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > prov.d <<EOF
/* Provider name is 53 chars long */
provider test_1234567890123456789012345678901234567890123_prov {
	probe go();
};
/* Provider name is 54 chars long */
provider test_12345678901234567890123456789012345678901234_prov {
	probe go();
};
EOF

$dtrace $dt_flags -G -s prov.d
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >& 2
	exit 1
fi

echo "DOF creation should have failed" >& 2
exit 0
