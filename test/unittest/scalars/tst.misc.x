#!/bin/sh

if ! $(grep -qw isofs /proc/kallmodsyms); then
    exit 1
fi

if ! $(grep -qw ext4 /proc/kallmodsyms); then
    exit 1
fi

exit 0
