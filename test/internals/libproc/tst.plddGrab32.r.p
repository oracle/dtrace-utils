#!/bin/sed -f
# Handle merged-usr systems where /lib == /usr/lib.
s:/usr/lib:/lib:g
