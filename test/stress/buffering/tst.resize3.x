#!/bin/sh
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Do not run this test under high load, to avoid any risk of timing out.

if [[ "$(cat /proc/loadavg | cut -d\  -f1 | cut -d\. -f1)" -gt 9 ]]; then
    exit 2
else
    exit 0
fi
