#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

set -e

echo "This script ($0) is meant to be run on an Ubuntu"
echo "installation, to facilitate building DTrace."
echo "If this system is not already running a DTrace enabled kernel, first"
echo "build and boot into an enabled kernel -- for example by running"
echo "prepare_kernel.sh."
echo
echo "Input the letter 'c' to continue with the build of DTrace,"
echo "otherwise this script will exit when the 'Enter' key is pressed."
echo "Press 'c', then press 'Enter' to \"continue\" with the build:"
read CONTINUE

if [ "$CONTINUE" != "c" ]
then
    echo "Exiting now per user input."
    exit 0
else
    echo "User indicated continuation of $0; beginning operations ..."
fi


echo "## cd to dtrace-utils:"
cd ../../../dtrace-utils

echo "## make dtrace-utils:"
# Build dtrace-utils -- build of utils is fairly quick
make SHELL=/bin/bash LIBDIR='/usr/local/lib' \
  > dists/Ubuntu/_makeDTraceUtilsOut.txt 2>&1

echo "To briefly explore the operation of DTrace, you may choose not to"
echo "install DTrace -- you can use $PWD/build/run-dtrace"
echo "as an equivalent, instead of 'dtrace'."
echo ""
echo "Quit this script to avoid installing DTrace."
echo ""
echo "To install so that DTrace can be used more easily, with the command"
echo "'dtrace', input the word 'install' to have this script install DTrace;"
echo "otherwise this script will exit when the 'Enter' key is pressed."
echo ""
echo "Input 'install', then press 'Enter' to \"install\" DTrace:"
read INSTALL

if [ "$INSTALL" != "install" ]
then
    echo "Exiting now per user input."
    exit 0
else
    echo "User indicated install of DTrace; beginning install ..."
fi

sudo make LIBDIR='/usr/local/lib' install
sudo /sbin/ldconfig  # access libdtrace.so.2
