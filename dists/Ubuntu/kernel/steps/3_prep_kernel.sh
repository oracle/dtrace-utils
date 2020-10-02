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

echo "## Creating kernel config file" | tee -a _000_stepsOut.txt

cd DTrace-Ubuntu-kernel

# Create Ubuntu config file; starting with Ubuntu standard build.
cat debian.master/config/config.common.ubuntu debian.master/config/amd64/config.common.amd64 debian.master/config/amd64/config.flavour.generic > .config

# fix or ensure some settings important to DTrace
echo "CONFIG_DTRACE=y"                    >> .config
echo "CONFIG_DT_CORE=m"                   >> .config
echo "CONFIG_DT_FASTTRAP=m"               >> .config
echo "CONFIG_DT_PROFILE=m"                >> .config
echo "CONFIG_DT_SDT=m"                    >> .config
echo "CONFIG_DT_SDT_PERF=y"               >> .config
echo "CONFIG_DT_FBT=m"                    >> .config
echo "CONFIG_DT_SYSTRACE=m"               >> .config
echo "CONFIG_DT_DT_TEST=m"                >> .config
echo "CONFIG_DT_DT_PERF=m"                >> .config
echo "CONFIG_DT_DEBUG=y"                  >> .config
echo "# CONFIG_DT_DEBUG_MUTEX is not set" >> .config

sed -i \
    -e 's/# CONFIG_UNWINDER_FRAME_POINTER is not set/CONFIG_UNWINDER_FRAME_POINTER=y/' \
    -e 's/CONFIG_UNWINDER_ORC=y/# CONFIG_UNWINDER_ORC is not set/' \
    -e 's/CONFIG_DEBUG_INFO_BTF=y/# CONFIG_DEBUG_INFO_BTF is not set/' \
    -e 's/CONFIG_DEBUG_INFO_DWARF4=y/# CONFIG_DEBUG_INFO_DWARF4 is not set/' \
    .config

echo "## running make oldconfig" | tee -a ../_000_stepsOut.txt
make oldconfig

# Identify kernel to be built as a DTrace enabled kernel.
sed -i -e 's/EXTRAVERSION =/EXTRAVERSION = -DTrace/' Makefile

cd ..

