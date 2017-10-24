#!/bin/sed -rf
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Rewrite reports that mention specific amount when lowering buffer limits.
s/lowered to [0-9][0-9]*([kmgt]| bytes)/lowered to #/g
