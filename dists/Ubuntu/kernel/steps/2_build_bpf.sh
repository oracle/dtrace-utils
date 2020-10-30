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

date | tee -a _000_stepsOut.txt

# leave binutils at master; no checkout required
echo "## starting build of BPF binutils" | tee -a _000_stepsOut.txt
mkdir -p _buildbinutils
cd _buildbinutils
# configure binutils for bpf
../srcbinut/configure --prefix=/usr/local --target=bpf-unknown-none \
    > ../_2cfgbinutout.txt 2>&1
make -j $(($(getconf _NPROCESSORS_ONLN)+1)) >> ../_2makebinutout.txt 2>&1
sudo make install >> ../_2makebinutout.txt 2>&1
cd ..

echo "## starting build of BPF gcc" | tee -a _000_stepsOut.txt
cd srcgcc/
git checkout releases/gcc-10
cd ..
mkdir -p _buildgcc
cd _buildgcc
# configure gcc for bpf
../srcgcc/configure --prefix=/usr/local --target=bpf-unknown-none \
    --disable-bootstrap > ../_2cfggccout.txt 2>&1
make -j $(($(getconf _NPROCESSORS_ONLN)+1)) >> ../_2makegccout.txt 2>&1
sudo make install >> ../_2makegccout.txt 2>&1
cd ..

date | tee -a _000_stepsOut.txt
