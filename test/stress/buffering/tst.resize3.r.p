#!/bin/sed -f
# Rewrite reports that mention specific amount when lowering buffer limits.
s/lowered to [0-9][0-9]*[kmgt]/lowered to #/g
