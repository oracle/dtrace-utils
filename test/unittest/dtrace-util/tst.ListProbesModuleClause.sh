#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# The -lm option can be used to list the probes from their module names.
# The presence of a (predicate or) clause is ignored.
#
# SECTION: dtrace Utility/-l Option;
# 	dtrace Utility/-m Option
##

dtrace=$1

$dtrace $dt_flags -lm vmlinux'/probefunc == "read"/{printf("FOUND");}' \
| gawk 'NF == 5 && $3 == "vmlinux" { print "success"; exit }'

exit 0
