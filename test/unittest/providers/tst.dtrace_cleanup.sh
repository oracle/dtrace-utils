#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
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

for iter in $(seq 1 $((timeout/2))); do
    sleep 1
    if grep -q :BEGIN $out; then
        iter=0
        break
    fi
done
if [[ $iter -ne 0 ]]; then
    echo error starting DTrace job
    cat $out
fi
rm -f $out

BEG0=`grep -c p:dt_${pid}_dtrace/BEGIN $UPROBE_EVENTS`
END0=`grep -c p:dt_${pid}_dtrace/END   $UPROBE_EVENTS`

kill $pid
wait $pid

BEG1=`grep -c p:dt_${pid}_dtrace/BEGIN $UPROBE_EVENTS`
END1=`grep -c p:dt_${pid}_dtrace/END   $UPROBE_EVENTS`

if [[ $iter -ne 0 || \
    $BEG0 != 1 || \
    $END0 != 1 || \
    $BEG1 != 0 || \
    $END1 != 0 ]]; then
	echo "actual: $BEG0 $END0 $BEG1 $END1"
	echo "expect: 1 1 0 0"
	exit 1
fi

exit 0
