#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2014, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This script tests that libproc can still track the state of the link map of a
# multithreaded process when it is continuously changing.  We turn LD_AUDIT off
# first to make sure that only one lmid is in use.
#
# The number of threads is arbitrary: it only matters that it is >1.
#

# @@timeout: 50

# @@xfail: almost guaranteed to fail due to loss of latching of process pre-libproc-mt

unset LD_AUDIT
export NUM_THREADS=4

exec test/triggers/libproc-consistency test/triggers/libproc-dlmadopen
