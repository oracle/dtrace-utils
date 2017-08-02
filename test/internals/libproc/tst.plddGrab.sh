#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This script tests that libproc can inspect the link maps of a subordinate
# process it grabs.
#

test/triggers/libproc-sleeper &
SLEEPER=$!
disown %+
while [[ $(readlink /proc/$SLEEPER/exe) =~ bash ]]; do :; done
test/triggers/libproc-pldd $SLEEPER
EXIT=$?
kill $SLEEPER
exit $EXIT
