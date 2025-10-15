#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Verify that the return() action can be used on the current kernel, i.e. that
# function error injection and BPF kprobe override is enabled.

if [[ -r /sys/kernel/debug/error_injection/list ]]; then
	exit 0
else
	exit 2
fi
