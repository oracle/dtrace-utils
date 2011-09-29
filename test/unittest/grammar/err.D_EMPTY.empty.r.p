#!/bin/sed -f
# We don't care what line number dtrace chooses to report when it
# diagnoses an empty translation unit.

s/line [0-9][0-9]*/line /
