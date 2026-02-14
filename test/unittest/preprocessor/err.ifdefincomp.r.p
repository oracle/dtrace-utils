#!/bin/sed -f
# GCC 16 improved diagnostics formatting.  Until GCC 16 is the minimum
# supported, we have to sed it back again.
s,[‘’],",g
