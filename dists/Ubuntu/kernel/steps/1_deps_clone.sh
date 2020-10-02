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

echo "## Beginning git clone of libdtrace-ctf ... " | tee -a _000_stepsOut.txt
git clone https://github.com/oracle/libdtrace-ctf.git > \
    _1clnLibDTraceCTFOut.txt 2>&1 &
CLNLCT=$!


echo "## installing bison" | tee -a _000_stepsOut.txt
sudo apt-get -y install bison >> _1_installs.txt

echo "## installing flex" | tee -a _000_stepsOut.txt
sudo apt-get -y install flex >> _1_installs.txt

echo "## installing libelf-dev" | tee -a _000_stepsOut.txt
sudo apt-get -y install libelf-dev >> _1_installs.txt

echo "## installing binutils-dev -- see sys/ctf_api.h, etc." \
  | tee -a _000_stepsOut.txt
sudo apt-get -y install binutils-dev >> _1_installs.txt

echo "## installing gcc-multilib -- see asm/types.h, etc." \
  | tee -a _000_stepsOut.txt
sudo apt-get -y install gcc-multilib >> _1_installs.txt

# Needed for libdtrace-ctf build
echo "## installing glib2.0" | tee -a _000_stepsOut.txt
sudo apt-get -y install glib2.0 >> _1_installs.txt

echo "## installing kernel-package" | tee -a _000_stepsOut.txt        # ELF
sudo apt-get -y install kernel-package >> _1_installs.txt
echo "## installing libdw-dev" | tee -a _000_stepsOut.txt             # dwarf
sudo apt-get -y install libdw-dev >> _1_installs.txt
echo "## installing libssl-dev" | tee -a _000_stepsOut.txt            # openssl
sudo apt-get -y install libssl-dev >> _1_installs.txt

echo "## installing gawk" | tee -a _000_stepsOut.txt      # gawk-isms are used
sudo apt-get -y install gawk >> _1_installs.txt

echo "## installing fakeroot" | tee -a _000_stepsOut.txt
sudo apt-get -y install fakeroot >> _1_installs.txt
echo "## installing fakeroot-ng" | tee -a _000_stepsOut.txt
sudo apt-get -y install fakeroot-ng >> _1_installs.txt
echo "## installing pseudo" | tee -a _000_stepsOut.txt
sudo apt-get -y install pseudo >> _1_installs.txt

echo "## installing libncurses5" | tee -a _000_stepsOut.txt
sudo apt-get -y install libncurses5 >> _1_installs.txt

echo "## installing libncurses5-dev" | tee -a _000_stepsOut.txt
sudo apt-get -y install libncurses5-dev >> _1_installs.txt

echo "## installing ccache" | tee -a _000_stepsOut.txt
sudo apt-get -y install ccache >> _1_installs.txt

echo "## installing glibc-source" | tee -a _000_stepsOut.txt
sudo apt-get -y install glibc-source >> _1_installs.txt

echo "## installing libasm-dev" | tee -a _000_stepsOut.txt
sudo apt-get -y install libasm-dev >> _1_installs.txt

echo "## installing libpcap-dev" | tee -a _000_stepsOut.txt
sudo apt-get -y install libpcap-dev >> _1_installs.txt

echo "## installing lib32gcc-9-dev" | tee -a _000_stepsOut.txt
sudo apt-get -y install lib32gcc-9-dev >> _1_installs.txt

echo "## installing nfs-kernel-server" | tee -a _000_stepsOut.txt
sudo apt-get -y install nfs-kernel-server >> _1_installs.txt

# Installation asks if non-superusers should have access to wireshark.
# Set to superuser only, with no prompt during installation.
echo "## installing tshark" | tee -a _000_stepsOut.txt
echo "wireshark-common wireshark-common/install-setuid boolean false" \
  | sudo debconf-set-selections
sudo DEBIAN_FRONTEND=noninteractive apt-get -y install tshark >> _1_installs.txt

if kill -0 "$CLNLCT" >/dev/null 2>&1; then
    echo clone of libdtrace-ctf still running | tee -a _000_stepsOut.txt
else
    echo NOT clone of libdtrace-ctf running | tee -a _000_stepsOut.txt
fi
date
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

if kill -0 "$CLNKRN" >/dev/null 2>&1; then
    echo clone of kernel still running | tee -a _000_stepsOut.txt
else
    echo NOT clone of kernel running | tee -a _000_stepsOut.txt
fi
date

wait $CLNKRN
echo "## Completed git clone of Linux kernel" | tee -a _000_stepsOut.txt
date
