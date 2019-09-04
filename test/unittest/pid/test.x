#!/bin/bash
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# err.* tests don't actually invoke anything, so the lack of a pid provider
# is not a problem.
[[ $_test =~ .**/err\. ]] && exit 0

# The same applies to restartloop.
[[ $_test =~ .**/tst\.restartloop ]] && exit 0

# UEK4 will not ever have the pid provider.
[[ $(uname -r | cut -d. -f 1-2) = 4.1 ]] && exit 1
exit 0
