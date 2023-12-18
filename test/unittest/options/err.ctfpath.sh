#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xdebug -xctfpath=/dev/null -n 'BEGIN { exit(0); }' |&
  awk '/Cannot open CTF archive \/dev\/null/ {
	sub(/^[^:]+: /, "");
	sub(/:.*$/, "");
	print;
	exit(1);
       }'

exit $?
