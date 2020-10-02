#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# See the kernel we start with.
echo
echo "Starting with a system running kernel:"
uname -r
echo

# Make sure we are running on x86_64 hardware
MACHINE=`uname -m`
echo "The machine running $0 is an" $MACHINE "system."
echo "These scripts only work properly while running on x86_64 systems."
if [ "$MACHINE" != "x86_64" ]
then
    echo "Wrong platform/processor!"
    echo "Script $0 is terminating now."
    echo
    exit 0
else
    echo "This is the correct platform/processor for this script."
    echo "Continuing with $0."
    echo
fi
echo "Press the 'Enter' key to continue:"
read CONTINUE

# This script installs a DTrace enabled kernel.
echo
echo "This script ($0) is meant to be run on an Ubuntu"
echo "installation, to facilitate building a Linux kernel with DTrace support."
echo
echo "To enable the use of DTrace, this script runs other scripts which:"
echo "  -- run 'update' and 'upgrade' on the Ubuntu system"
echo "  -- install packages (e.g. dependencies for the kernel build)"
echo "  -- git-clone and build an Ubuntu kernel with DTrace support"
echo "  -- install the built kernel"
echo "  -- reboot the system to the built kernel"
echo "These operations are significant and the scripts were written with the"
echo "assumption the user is familiar with the implications."
echo "Only continue with this script if you are comfortable with the operations"
echo "which will be run and you are willing to be responsible for the effects"
echo "of running these scripts."
echo "Read this script ($0) and scripts under"
echo "$PWD/kernel/steps"
echo "for details and specifics about the operations that will be performed."
echo
echo "This script builds a DTrace enabled kernel."
echo "Input the letter 'c' if you agree to these operations being performed,"
echo "otherwise this script will exit when the 'Enter' key is pressed."
echo "Press 'c', then press 'Enter' to \"continue\" the script:"
read CONTINUE

if [ "$CONTINUE" != "c" ]
then
    echo "Exiting now per user input."
    exit 0
else
    echo "User indicated continuation of $0; beginning operations ..."
fi


# Check storage availability.
PWD=`pwd`

# Find how much space is remaining on the partition of this directory.
RMNSPC=`df -h $PWD | awk '{ if ($4 != "Avail") print $4 }'`

echo ""
echo "The required package installations, source code, and DTrace enabled"
echo "kernel build consume about 30G of storage space, when starting from a" 
echo "fresh install of Ubuntu."
echo "There appears to be $RMNSPC space remaining in the partition where"
echo "this script is running."

for STEP in ./kernel/steps/*.sh; do
    echo
    echo "=========="
    echo "Enter:"
    echo "  's' to skip $STEP;"
    echo "  'q' to quit $0"
    echo "  anything else will run $STEP (default)"
    read INPUT
    if [ "$INPUT" == "s" ]; then
	echo "Skipping $STEP per user input 's'"
        continue
    fi
    if [ "$INPUT" == "q" ]; then
	echo "Exiting $0 per user input 'q'"
        exit 0
    fi
    $STEP
    if [ $? -ne 0 ]; then
        echo "DTrace step script $STEP returned an error."
	echo "Exiting $0"
        exit 1
    fi
    echo "complete" >> ./kernel/_00_steprunning.txt
done
