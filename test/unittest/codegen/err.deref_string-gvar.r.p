#!/usr/bin/sed -f

# runtest.sh looks for "0x" to filter out pointer values.
# Strip the 0x so that the illegal address will not be filtered out;
# we want the address to be checked.
s/0x//
