#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This script tests that libproc can inspect the link maps of a subordinate
# process it creates.
#

test/triggers/libproc-pldd test/triggers/libproc-sleeper
