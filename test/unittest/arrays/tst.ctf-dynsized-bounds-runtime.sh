#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# ASSERTION: Array accesses work for CTF-declared arrays of dynamic size
#            (ensuring the bounds checking is also bypassed at runtime).
#
# SECTION: Pointers and Arrays/Array Declarations and Storage

dtrace=$1

# The mm_struct 'cpu_bitmap' was renamed to 'flexible_array', so we need to
# see which member name the current kernel uses.  Since it is always the last
# member of mm_struct, we just use that member name, and hope for the best.
# If mm_struct ever gets changed to not have a flexible array at its end, this
# test will need a new structure to work with.

# Determine the member name we should use.
member=`bpftool btf dump id 1 | \
		awk '/^\[[0-9]+\] STRUCT \047mm_struct\047/ { in_mm = 1; next; }
		     /^\t/ && in_mm { fld = $1; gsub(/\047/, "", fld); next }
		     /^\[[0-9]+\] [_A-Z]+ \047/ && in_mm { print fld; exit; }'`


# Try to access the flexible array.
$dtrace $dt_flags -qn "
BEGIN
{
	i = pid - pid;			/* non-constant 0 value */
	v = curthread->mm->$member[i];
	exit(0);
}

ERROR
{
	exit(1);
}"

echo $?
