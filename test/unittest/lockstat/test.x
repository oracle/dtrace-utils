#!/bin/bash
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
if ! check_provider lockstat; then
        echo "Could not load lockstat provider"
        exit 1
fi
exit 0
