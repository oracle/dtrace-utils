#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

myversion=`$dtrace $dt_flags -V | awk '{ print $NF }'`
echo version is $myversion

$dtrace $dt_flags -xversion=$myversion -qn 'BEGIN { exit(0) }'
exit $?
