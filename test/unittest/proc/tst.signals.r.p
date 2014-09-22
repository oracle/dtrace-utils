#!/bin/sh
#
# Eliminate blank lines and the monitor messages emitted by bash.
# (Turning off monitor mode should quash these, but it doesn't seem
# to work.)

grep -v '^$' | grep -vE '^test/unittest/proc/tst\.signals\.sh: line [0-9]+: *[0-9]+ '
