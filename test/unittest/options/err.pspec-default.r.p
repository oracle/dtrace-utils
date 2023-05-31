#!/bin/sed -f
# Chop off everything after the 'near' in the error message, to evade a bug
# at EOF in our scanner under flex < 2.6.
s/syntax error near.*$/syntax error near/g
