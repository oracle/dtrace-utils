#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This script tests that libproc can still track the state of the link map when
# it is continuously changing, even when lmids != 0 are in use.
#

# @@timeout: 50
# @@tags: unstable

unset LD_AUDIT
export MANY_LMIDS=t
exec test/triggers/libproc-consistency test/triggers/libproc-dlmadopen
