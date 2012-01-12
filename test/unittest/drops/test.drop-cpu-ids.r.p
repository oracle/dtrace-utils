#!/bin/sed -f
# Drop reports have a variable CPU ID in them.
s/CPU [0-9][0-9]*/CPU #/g
