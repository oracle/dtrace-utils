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

echo "## Setting default boot to DTrace enabled kernel" | \
  tee -a _000_stepsOut.txt

GDF="GRUB_DEFAULT="
BOOT_0_DF="$GDF"0
KERNEL=`cat _2kernel-build-version`-DTrace+
DTRACE_DF="$GDF\"Advanced options for Ubuntu>Ubuntu, with Linux $KERNEL\""

echo "Changing default boot kernel to $KERNEL."
sudo sed -i -e s/"$BOOT_0_DF"/"$DTRACE_DF"/ /etc/default/grub

sudo update-grub

echo ""
echo "NOTE: Run build_dtrace.sh from $PWD"
echo "after rebooting into the DTrace enabled kernel."
echo ""
echo "Reboot into DTrace enabled kernel?  ENTERING 'r' WILL REBOOT THIS SYSTEM!"
read REBOOT
if [ $REBOOT = 'r' ]
then
    echo "Rebooting now!"
    sudo reboot
else
    echo "The user input was not 'r'."
    echo "Manually reboot to run the DTrace enabled kernel -- e.g. with:"
    echo "'sudo reboot'."
fi

