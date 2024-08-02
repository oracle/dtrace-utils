#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# @@nosort

dtrace=$1

function mytest() {
	echo try $1
	$dtrace $1 -qn 'BEGIN { trace((string)&`linux_banner); exit(0); }' | cut -c-14
}

# Test different link modes.
# - (default) should be the same as "kernel"
# - "static" means that all symbols are static
# - "dynamic" means that all symbols are dynamic
# - "kernel" means that kernel symbols are static while user symbols are dynamic
# - "foo" would be unrecognized

# But no DTrace (even Solaris) supported userspace symbol resolving.
# So (default), "static", and "kernel" should all be the same.
# They should all work.
mytest " "
mytest "-xlinkmode=static"
mytest "-xlinkmode=kernel"

# These should not work.
mytest "-xlinkmode=foo"
mytest "-xlinkmode=dynamic"

# This should work.
mytest "-xlinkmode=dynamic -xknodefs"

exit 0
