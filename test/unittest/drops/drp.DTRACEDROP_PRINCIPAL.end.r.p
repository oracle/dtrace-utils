#!/bin/sed -f
# Rewrite reports that have a variable CPU ID in them to make them generic.
s/CPU [0-9][0-9]*/CPU #/g
