#!/bin/bash

#
# In older kernels (e.g., UEK6), the BPF helper function had a bug with
# skipping stack frames.  While that many stack frames were correctly
# skipped on the leaf end of the stack, that many frames were also
# incorrectly skipped at the root end of the output buffer.
#
# Only two DTrace providers ask for a nonzero skip count.  One is the
# fentry/fexit implementation of fbt, but it was not used on such older
# kernels.
#
# The other is rawtp.  Therefore, rawtp stack(), caller, and stackdepth
# results on such older kernels can be incorrect.
#
# A few other providers also have some or most probes implemented in
# terms of rawtp probes.  They could have similar problems, depending
# on which probes are used.
#

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -gt 5 ]; then
	exit 0
fi
if [ $MAJOR -eq 5 -a $MINOR -ge 10 ]; then
	exit 0
fi

echo "rawtp caller, stack(), and stackdepth problematic in old kernels"
exit 2
