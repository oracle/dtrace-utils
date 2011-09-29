#!/bin/sed -f
# Strip out pointers: these derive from a stackdump and have unpredictable depth.
/0x[0-9a-f][0-9a-f]*/d
