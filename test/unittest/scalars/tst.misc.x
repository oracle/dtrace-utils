#!/bin/sh

if [ -e /proc/kallmodsyms ]; then
	if ! grep -q 'isofs_dir_operations.*isofs' /proc/kallmodsyms; then
		exit 1
	fi

	if ! grep -q 'ext4_dir_operations.*ext4' /proc/kallmodsyms; then
		exit 1
	fi
	exit 0
fi

if ! grep -qw isofs_dir_operations /proc/kallsyms; then
	exit 1
fi
if ! grep -qw ext4_dir_operations /proc/kallsyms; then
	exit 1
fi
exit 0
