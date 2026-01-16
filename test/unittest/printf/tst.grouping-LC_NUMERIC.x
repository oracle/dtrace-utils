#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

if locale -a | grep -q en_US.utf8 ; then
	exit 0
fi

echo the en_US.utf8 locale was not found
exit 2
