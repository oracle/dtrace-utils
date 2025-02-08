#!/bin/sh

# A pid entry probe places a uprobe on the first instruction of a function.
# Unfortunately, this is so early in the function preamble that the function
# frame pointer has not yet been established and the actual caller of the
# traced function is missed.
#
# In Linux 6.11, x86-specific heuristics are introduced to fix this problem.
# See commit cfa7f3d
# ("perf,x86: avoid missing caller address in stack traces captured in uprobe")
# for both a description of the problem and an explanation of the heuristics.
#
# Add post processing to these test results to allow for both cases:
# caller frame is missing or not missing.

missing_caller=1
if [ $(uname -m) == "x86_64" ]; then
        read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

        if [ $MAJOR -ge 6 ]; then
                if [ $MAJOR -gt 6 -o $MINOR -ge 11 ]; then
                        missing_caller=0
                fi
        fi
fi

if [ $missing_caller -eq 1 ]; then
        # Add the missing caller function after the current function.
        awk '{ print }
             /myfunc_z/ { print "              ustack-tst-basic`myfunc_y+{ptr}" }'
else
        # The .r results file has an extra frame at the end in case
        # the caller frame is missing and the 25-frame limit goes
        # "too far."  If the caller is not missing, fake that extra frame.
        awk '{ print }
             /myfunc_b/ { print "              ustack-tst-basic`myfunc_a+{ptr}" }'
fi
