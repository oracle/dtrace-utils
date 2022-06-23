#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

#
# We test whether the -xcpppath option works by replacing the pre-processor
# executable to be used with 'cat'.  This will trigger an error due to the
# default options passed to the pre-processor which conclusively shows that
# the cpppath option took effect.
#
$dtrace $dt_flags -xcpppath=/bin/cat -Cs /dev/stdin << EOT
#if 1
BEGIN
{
	exit(0);
}
#endif
EOT

# We expect an error to be reported.
if [ $? -eq 0 ]; then
	exit 1
else
	exit 0
fi
