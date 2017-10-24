#!/bin/sh
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# df --output=source is quite new: skip if it doesn't work.
if ! df --output=source >/dev/null 2>&1; then
    exit 1
fi
