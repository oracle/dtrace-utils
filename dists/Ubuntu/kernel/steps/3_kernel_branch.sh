#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# This script offers no error checking for user input.  The assumption is the
# user will select valid values for the version of the additional tracing
# features and the Ubuntu kernel version.
# Also, the assumption is the user will select a tracing features version
# less than or equal to the Ubuntu kernel version.

set -e
cd kernel
echo $0 > _00_steprunning.txt

date | tee -a _000_stepsOut.txt

# Glean known good kernels as suggestions for user
UKPROMPT=`cat ./steps/eg_ubuntu_kern`
DKPROMPT=`cat ./steps/eg_dtrace_kern`

# Collect information now -- select branches and add tracing and Ubuntu commits.
cd DTrace-Ubuntu-kernel

# Source Ubuntu kernel commits
git remote add Ubuntu https://git.launchpad.net/~ubuntu-kernel-test/ubuntu/\
+source/linux/+git/mainline-crack
echo "## Beginning update of Ubuntu source remote" | \
  tee -a ../_000_stepsOut.txt
git remote update Ubuntu >> ../_3ubupout.txt 2>&1

# Source tracing feature commits for DTrace to use
echo ""
git remote add dtrace https://github.com/oracle/dtrace-linux-kernel
echo "## Beginning update of kernel source with additional tracing features." |\
  tee -a ../_000_stepsOut.txt
git remote update dtrace >> ../_3dtupout.txt 2>&1
echo "Listing of kernel branches with additional tracing features:" | \
  tee -a ../_000_stepsOut.txt
git remote show dtrace | grep "v2/"

# See Ubuntu kernel source here: https://kernel.ubuntu.com/~kernel-ppa/mainline/
# and pick checkout version based on the kernel version we intend to build.
echo "## Select Ubuntu kernel source branch" | tee -a ../_000_stepsOut.txt
echo "Consider using a browser to see the Ubuntu kernels available at:"
echo "  https://kernel.ubuntu.com/~kernel-ppa/mainline/"
echo "Note the kernel versions in the list above, which will be needed"
echo "to find a near match with the Ubuntu kernel chosen here."
echo "TracingKernelVersion <= UbuntuKernelVersion"
echo "Enter Ubuntu kernel version numbers here (e.g. $UKPROMPT):"
read UKERNEL

# Save the kernel version chosen, for later reference (selecting reboot kernel)
echo "Ubuntu kernel version selected $UKERNEL" | tee -a ../_000_stepsOut.txt
echo $UKERNEL > ../_3kernel-build-version

# git ls-remote --tags Ubuntu -l *cod*v$UKERNEL
echo "## Fetching Ubuntu kernel tag" | tee -a ../_000_stepsOut.txt
git fetch Ubuntu "refs/tags/cod/mainline/v$UKERNEL:cod/mainline/v$UKERNEL"

echo "## Checking-out kernel version" | tee -a ../_000_stepsOut.txt
git checkout v$UKERNEL -b DTraceUbuntu_$UKERNEL

echo "## Merging Ubuntu commits" | tee -a ../_000_stepsOut.txt
git merge --no-edit cod/mainline/v$UKERNEL >> ../_3ubmrgout.txt 2>&1

# Choose the appropriate version providing additional tracing features to use
# as a code source.
# Pick checkout version based on the kernel version we intend to build.
echo "The kernel version from the additional tracing feature repository must be"
echo "equal to or less than Ubuntu version $UKERNEL.  If there is no choice"
echo "equal, choose the nearest version less than the Ubuntu version"
echo "(previously chosen)."
echo "See the output above for the additional tracing feature kernel versions"
echo "available."
echo "Enter additional tracing features kernel version numbers here"
echo "(e.g. $DKPROMPT):"
read DKERNEL
echo "Tracing version selected $DKERNEL" | tee -a ../_000_stepsOut.txt

echo "## Merging commits for additional tracing features" | \
  tee -a ../_000_stepsOut.txt
git merge --no-edit dtrace/v2/$DKERNEL | tee -a ../_3dtmrgout.txt 2>&1

cd ..

date | tee -a _000_stepsOut.txt
