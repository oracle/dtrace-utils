#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

DIRNAME="$tmpdir/ldpath.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# ldpath=ld does nothing remarkable, but this test complements err.ldpath.d
$dtrace $dt_flags -G -xldpath=ld -n 'BEGIN {exit(0)}'
exit $?
