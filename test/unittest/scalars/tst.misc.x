#!/bin/sh

if ! grep -qw isofs_dir_operations /proc/kallsyms; then
    exit 1
fi

if ! grep -qw ext4_dir_operations /proc/kallsyms; then
    exit 1
fi

exit 0
