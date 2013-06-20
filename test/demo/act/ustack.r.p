#!/bin/bash
#
# Condense out lines from ld.so and lines with no symbols.
# The remainder should be fairly invariant across libc versions.

grep -v -e 'ld-linux-x86-64\.so\.2`' -e '`0x[0-9a-f][0-9a-f]*'
