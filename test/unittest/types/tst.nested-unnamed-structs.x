#!/bin/sh

if ! $(grep -q '^rds ' /proc/modules); then
    exit 1
fi

exit 0
