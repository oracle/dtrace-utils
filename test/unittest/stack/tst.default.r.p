#!/bin/sed -nf

# Eliminate all lines other than vmlinux`dtrace_stacktrace and unresolved
# addresses.

/dtrace`dtrace_state_go/p
/[^+]0x[0-9a-f]\+$/p
