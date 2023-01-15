#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Form a list of events to test on this system.
# Use at least cpu-clock but also try "perf list" for some others.

eventnamelist="cpu-clock"
for eventname in branches instructions; do
	if perf list hw | grep -qw $eventname; then
		eventnamelist="$eventnamelist $eventname"
	fi
done

echo $eventnamelist
