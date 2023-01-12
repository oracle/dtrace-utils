#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Temporarily skip due to some bug we do not yet understand.  It seems cpc
# probes fire only once per process on x86/UEKR6 when using hardware counters.

if [[ `uname -m` != "x86_64" ]]; then
        exit 0
fi

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -gt 5 ]; then
        exit 0
fi
if [ $MAJOR -eq 5 -a $MINOR -ge 15 ]; then
        exit 0
fi

echo mystery bug with hardware counters on x86 and UEKR6
exit 2
