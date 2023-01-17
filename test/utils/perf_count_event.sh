#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# count some event for some executable

# get the test/utils directory name
utils=`dirname $0`

# get the event to count
event=$1

# the rest of the command line is the executable and its arguments
shift

# Use "perf stat" to count "event" for this executable and its children.
#   If the output is no good, report -1.
#   If the output is time in msec, convert to nsec.
#   Otherwise, just report the count.
perf stat -e $event --no-big-num -x\  $utils/$* |& awk '
/^[^0-9]/ { print -1; exit 1 }
/ msec / { print int(1000000. * $1); exit 0 }
{ print $1; exit 0 }'

exit 0
