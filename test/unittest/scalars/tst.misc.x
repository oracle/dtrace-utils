#!/bin/sh

if ! $(grep -q '^isofs ' /proc/modules); then
    exit 1
fi

if ! $(grep -q '^ext4 ' /proc/modules); then
    exit 1
fi

exit 0
