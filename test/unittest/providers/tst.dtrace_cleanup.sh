#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

##
#
# ASSERTION:
# The dtrace probes BEGIN and END will create pid-tagged uprobes that
# will be cleaned up when the dtrace process exits.
#
##

dtrace=$1
UPROBE_EVENTS=/sys/kernel/debug/tracing/uprobe_events

out=/tmp/output.$$
$dtrace $dt_flags -n BEGIN,END &>> $out &
pid=$!

tail -F $out | awk '/matched [1-9] probe/ { exit; }'
rm -f $out

BEG0=`grep -c p:dt_${pid}_dtrace/BEGIN $UPROBE_EVENTS`
END0=`grep -c p:dt_${pid}_dtrace/END   $UPROBE_EVENTS`

kill $pid
wait $pid

BEG1=`grep -c p:dt_${pid}_dtrace/BEGIN $UPROBE_EVENTS`
END1=`grep -c p:dt_${pid}_dtrace/END   $UPROBE_EVENTS`

if [[ $BEG0 != 1 || \
     $END0 != 1 || \
     $BEG1 != 0 || \
     $END1 != 0 ]]; then
	echo "actual: $BEG0 $END0 $BEG1 $END1"
	echo "expect: 1 1 0 0"
	exit 1
fi

exit 0
