#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

set -e
cd kernel
echo $0 > _00_steprunning.txt

# Go to kernel source code
cd DTrace-Ubuntu-kernel

echo "## Beginning kernel build; takes some time" | tee -a ../_000_stepsOut.txt
date | tee -a ../_000_stepsOut.txt
make -j $(($(getconf _NPROCESSORS_ONLN)+1)) > ../_5makeDTKernOut.txt 2>&1

echo "## Beginning CTF generation" | tee -a ../_000_stepsOut.txt
date | tee -a ../_000_stepsOut.txt
make ctf SHELL=/bin/bash -j $(($(getconf _NPROCESSORS_ONLN)+1)) \
  > ../_5makeCtfOut.txt 2>&1

echo "## Beginning headers_install" | tee -a ../_000_stepsOut.txt
make headers_install > ../_5makeHdrInst.txt 2>&1
date | tee -a ../_000_stepsOut.txt

echo "## Beginning make modules_install" | tee -a ../_000_stepsOut.txt
date | tee -a ../_000_stepsOut.txt
sudo make modules_install > ../_5makeModsInst.txt 2>&1

echo "## Beginning make install" | tee -a ../_000_stepsOut.txt
date | tee -a ../_000_stepsOut.txt
sudo make install | tee -a ../_5makeInstall.txt

date | tee -a ../_000_stepsOut.txt
