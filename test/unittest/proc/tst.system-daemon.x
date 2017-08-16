#!/bin/bash
# Oracle Linux DTrace.
# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

if [[ ! -f /var/run/syslogd.pid ]] ||
   [[ ! -d "/proc/$(cat /var/run/syslogd.pid)" ]] ||
   { grep -qF '(deleted)' "/proc/$(cat /var/run/syslogd.pid)/maps" &&
     [[ ! -d "/proc/$(cat /var/run/syslogd.pid)/map_files" ]]; }; then
    exit 1
fi

exit 0
