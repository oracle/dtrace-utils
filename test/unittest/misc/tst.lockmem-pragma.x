#!/bin/bash

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -lt 5 ]; then
        exit 0
fi
if [ $MAJOR -eq 5 -a $MINOR -lt 15 ]; then
        exit 0
fi

# Somehow, UEKR6 (5.4.17) has problems with the the locked-memory limit,
# but UEKR7 (5.15.0) does not

echo "no locked-memory limit on newer kernels?"
exit 1
