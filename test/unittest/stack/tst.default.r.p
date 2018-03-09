#!/bin/sed -nf

# Eliminate all lines other than dtrace`ioctl.

/dtrace`dtrace_ioctl/p
