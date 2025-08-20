#!/bin/bash

# SYNOPSIS
#   sudo ./300actions-exit.sh
#
# DESCRIPTION
#   The exit() action terminates the script, returning an
#   unsigned 8-bit integer value.

/usr/sbin/dtrace -n '
BEGIN
{
	x = 123;
	y = 456;
	exit(x < y ? x : y);
}'

echo $?
