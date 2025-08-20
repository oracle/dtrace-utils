#!/bin/bash

#  SYNOPSIS
#    ./102list_probe_arguments.sh
#
#  DESCRIPTION
#    Some providers provide typed arguments for (some of) their
#    probes.  Use "dtrace -lv" for a verbose listing to see the
#    arguments and their types, if any.

sudo /usr/sbin/dtrace -lv -n 'syscall::open:entry'
