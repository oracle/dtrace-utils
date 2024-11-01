#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xbpflog -qn 'BEGIN { exit(0); }' |& grep '^BPF: ' > /dev/null

exit $?
