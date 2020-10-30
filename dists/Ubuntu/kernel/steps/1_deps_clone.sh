#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# Install dependency packages and git-clone source.  Try to use cloning time
# efficiently by running package installs, checkout and build libdtrace-ctf.

set -e
cd kernel
echo $0 > _00_steprunning.txt

echo "## installing build-essential" | tee -a _000_stepsOut.txt
sudo apt-get -y install build-essential >> _1_installs.txt

echo "## Beginning git clone of Linux kernel ... " | tee -a _000_stepsOut.txt
STABLE="https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git"
git clone $STABLE DTrace-Ubuntu-kernel > _1clnDTraceKernOut.txt 2>&1 &
CLNKRN=$!

echo "## Beginning git clone of binutils ... " | tee -a _000_stepsOut.txt
git clone https://sourceware.org/git/binutils-gdb.git srcbinut > \
    _1clnBinOut.txt 2>&1 &
CLNBIN=$!

echo "## Beginning git clone of gcc ... " | tee -a _000_stepsOut.txt
git clone https://gcc.gnu.org/git/gcc.git srcgcc  > _1clnGccOut.txt 2>&1 &
CLNGCC=$!

echo "## Beginning git clone of libdtrace-ctf ... " | tee -a _000_stepsOut.txt
git clone https://github.com/oracle/libdtrace-ctf.git > \
    _1clnLibDTraceCTFOut.txt 2>&1 &
CLNLCT=$!


# Needed for binutils build
echo "## installing texinfo" | tee -a _000_stepsOut.txt
sudo apt-get -y install texinfo >> _1_installs.txt
echo "## installing bison" | tee -a _000_stepsOut.txt
sudo apt-get -y install bison >> _1_installs.txt
echo "## installing flex" | tee -a _000_stepsOut.txt
sudo apt-get -y install flex >> _1_installs.txt

# Needed for gcc build
echo "## installing libgmp-dev" | tee -a _000_stepsOut.txt
sudo apt-get -y install libgmp-dev >> _1_installs.txt
echo "## installing libmpc-dev" | tee -a _000_stepsOut.txt
sudo apt-get -y install libmpc-dev >> _1_installs.txt

# Needed for libdtrace-ctf build
echo "## installing glib2.0" | tee -a _000_stepsOut.txt
sudo apt-get -y install glib2.0 >> _1_installs.txt

# For kernel build
echo "## installing kernel-package" | tee -a _000_stepsOut.txt        # ELF
sudo apt-get -y install kernel-package >> _1_installs.txt
echo "## installing libdw-dev" | tee -a _000_stepsOut.txt             # dwarf
sudo apt-get -y install libdw-dev >> _1_installs.txt
echo "## installing libssl-dev" | tee -a _000_stepsOut.txt            # openssl
sudo apt-get -y install libssl-dev >> _1_installs.txt

echo "## installing gawk" | tee -a _000_stepsOut.txt
sudo apt-get -y install gawk >> _1_installs.txt

echo "## installing binutils-dev -- see sys/ctf_api.h, etc." \
  | tee -a _000_stepsOut.txt
sudo apt-get -y install binutils-dev >> _1_installs.txt

wait $CLNLCT
echo "## Completed clone of libdtrace-ctf; build ..." | tee -a _000_stepsOut.txt
cd libdtrace-ctf
# Pass in the install location because Ubuntu doesn't use default /usr/lib64
make prefix=/usr/local LIBDIR='$(prefix)/lib'
sudo make prefix=/usr/local LIBDIR='$(prefix)/lib' install
sudo /sbin/ldconfig
cd ..

# Confirm git has configuration so that merge operation will work:
GITSTATUS="bad"
while [ $GITSTATUS != "good" ]; do
    GITSTATUS="good"
    if ! git config user.email > /dev/null; then
        echo
        echo error: git user.email is not set
        GITSTATUS="bad"
    fi
    if ! git config user.name > /dev/null; then
        echo
        echo error: git user.name is not set
        GITSTATUS="bad"
    fi
    if [ $GITSTATUS != "good" ]; then
        echo
        echo "'git user.email' and 'git user.name' must be set to proceed"
        echo "with merging commits to the kernel source."
        echo "git will not merge commits without these values set."
        echo "Example lines that would set these values, including '\"', are:"
        echo "    git config --global user.email \"your.email@domain.you\""
        echo "    git config --global user.name \"Your Name\""
        echo "User:"
        whoami
        echo "Network address should be somewhere here:"
        ip addr | grep inet
        echo "Consider setting the git values in another terminal, then"
        echo "press 'Enter' in this terminal."
        read NOMINAL
    fi
done


wait $CLNBIN
echo "## Completed git clone of binutils" | tee -a _000_stepsOut.txt
if kill -0 "$CLNGCC" >/dev/null 2>&1; then
    echo clone of gcc still running | tee -a _000_stepsOut.txt
else
    echo NOT clone of gcc running | tee -a _000_stepsOut.txt
fi
if kill -0 "$CLNKRN" >/dev/null 2>&1; then
    echo clone of kernel still running | tee -a _000_stepsOut.txt
else
    echo NOT clone of kernel running | tee -a _000_stepsOut.txt
fi
date | tee -a _000_stepsOut.txt

wait $CLNKRN
echo "## Completed git clone of Linux kernel" | tee -a _000_stepsOut.txt
date | tee -a _000_stepsOut.txt

wait $CLNGCC
echo "## Completed git clone of gcc" | tee -a _000_stepsOut.txt
date | tee -a _000_stepsOut.txt
