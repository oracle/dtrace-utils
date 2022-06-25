#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

#
# First determine the location of the vmlinux CTF archive.
#
ctf=`$dtrace $dt_flags -xdebug |&
	awk '/Loaded shared CTF from/ { $0 = $NF; sub(/\.$/, ""); print; }'`

$dtrace $dt_flags -xctfpath=$ctf -n 'BEGIN { exit(0); }'

exit $?
