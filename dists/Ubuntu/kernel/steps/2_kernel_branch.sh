#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# This script offers no error checking for user input.  The assumption is the
# user will select valid values for the DTrace kernel version and the Ubuntu
# kernel version.  Also, the assumption is the user will select a
# DTrace-kernel-version-number less than or equal to the Ubuntu-kernel-version.

set -e
cd kernel
echo $0 > _00_steprunning.txt

date

# Glean known good kernels as suggestions for user
UKPROMPT=`cat ./steps/eg_ubuntu_kern`
DKPROMPT=`cat ./steps/eg_dtrace_kern`

# Collect information now -- select branches and add DTrace and Ubuntu commits.
cd DTrace-Ubuntu-kernel

# Source Ubuntu kernel commits
git remote add Ubuntu https://git.launchpad.net/~ubuntu-kernel-test/ubuntu/\
+source/linux/+git/mainline-crack
echo "## Beginning update of Ubuntu source remote" | \
  tee -a ../_000_stepsOut.txt
git remote update Ubuntu >> ../_2ubupout.txt 2>&1

# Source DTrace commits
echo ""
git remote add dtrace https://github.com/oracle/dtrace-linux-kernel
echo "## Beginning update of DTrace source remote" | \
  tee -a ../_000_stepsOut.txt
git remote update dtrace >> ../_2dtupout.txt 2>&1
echo "Listing of DTrace kernel source branches for DTrace:" | tee -a \
  ../_000_stepsOut.txt
git remote show dtrace | grep "v1/"

# See Ubuntu kernel source here: https://kernel.ubuntu.com/~kernel-ppa/mainline/
# and pick checkout version based on the kernel version we intend to build.
echo "## Select Ubuntu kernel source branch" | tee -a ../_000_stepsOut.txt
echo "Consider using a browser to see the Ubuntu kernels available at:"
echo "  https://kernel.ubuntu.com/~kernel-ppa/mainline/"
echo "Note the DTrace kernels in the list above, which will be needed for"
echo "a near match with the Ubuntu kernel chosen here."
echo "DTraceKernelVersion <= UbuntuKernelVersion"
echo "Enter Ubuntu kernel version numbers here (e.g. $UKPROMPT):"
read UKERNEL

# Save the kernel version chosen, for later reference (selecting reboot kernel)
echo "Ubuntu kernel version selected $UKERNEL" | tee -a ../_000_stepsOut.txt
echo $UKERNEL > ../_2kernel-build-version

# git ls-remote --tags Ubuntu -l *cod*v$UKERNEL
echo "## Fetching Ubuntu kernel tag" | tee -a ../_000_stepsOut.txt
git fetch Ubuntu "refs/tags/cod/mainline/v$UKERNEL:cod/mainline/v$UKERNEL"

echo "## Checking-out kernel version" | tee -a ../_000_stepsOut.txt
git checkout v$UKERNEL -b DTraceUbuntu_$UKERNEL

echo "## Merging Ubuntu commits" | tee -a ../_000_stepsOut.txt
git merge --no-edit cod/mainline/v$UKERNEL >> ../_2ubmrgout.txt 2>&1

# Choose the appropriate DTrace kernel to use as a code source.
# Pick checkout version based on the kernel version we intend to build.
echo "The kernel version from the DTrace repository must be equal to or less"
echo "than Ubuntu version $UKERNEL.  If there is no choice equal, choose the"
echo "nearest version less than the Ubuntu version (previously chosen)."
echo "See the output above for DTrace kernel versions available."
echo "Enter DTrace kernel version numbers here (e.g. $DKPROMPT):"
read DKERNEL
echo "DTrace kernel version selected $DKERNEL" | tee -a ../_000_stepsOut.txt

echo "## Merging DTrace commits" | tee -a ../_000_stepsOut.txt
git merge --no-edit dtrace/v1/$DKERNEL | tee -a ../_2dtmrgout.txt 2>&1

cd ..

date
