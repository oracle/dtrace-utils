#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# If /proc/kallmodsyms does not exist, there is nothing to test

[[ -r /proc/kallmodsyms ]] || exit 2

exit 0
