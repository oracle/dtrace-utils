#!/bin/bash
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# UEK4 will not ever have the pid provider.
[[ $(uname -r | grep -Eo '^[0-9]+\.[0-9]+') = 4.1 ]] && exit 1
exit 0
