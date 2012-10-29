#!/bin/sed -nf

# Eliminate all lines other than vmlinux`dtrace_stacktrace and unresolved
# addresses.

/vmlinux`dtrace_stacktrace/p
/[^+]0x[0-9a-f]\+$/p
