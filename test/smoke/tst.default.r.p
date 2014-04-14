#!/bin/sed -nf

# Eliminate all lines other than dtrace`dtrace_state_go and unresolved
# addresses.

/dtrace`dtrace_state_go/p
/[^+]0x[0-9a-f]\+$/p
