#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION: The -lm option indicates nonexistent module names.
#
# SECTION: dtrace Utility/-lm Option
#
##

dtrace=$1

$dtrace $dt_flags -lm foobar
exit $?
