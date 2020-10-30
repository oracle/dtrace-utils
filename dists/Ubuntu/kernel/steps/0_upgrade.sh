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

# Get current.
date | tee -a _000_stepsOut.txt
echo "## Beginning system update:" | tee -a _000_stepsOut.txt
sudo apt-get -y update >> _000_stepsOut.txt
date | tee -a _000_stepsOut.txt
echo "## Beginning upgrade:" | tee -a _000_stepsOut.txt
sudo apt-get -y upgrade >> _000_stepsOut.txt
date | tee -a _000_stepsOut.txt
