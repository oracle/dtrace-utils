#!/bin/bash

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

if [ $(uname -m) == "x86_64" ]; then
        read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

        if [ $MAJOR -ge 6 ]; then
                if [ $MAJOR -gt 6 -o $MINOR -ge 11 ]; then
                        awk '{ sub("myfunc_w", "myfunc_v"); print; }'
                        exit 0
                fi
        fi
fi

# Otherwise, just pass the output through.
cat
