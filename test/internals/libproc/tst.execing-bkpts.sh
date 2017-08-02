#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2014, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This script tests that dropping a breakpoint on a process that is constantly
# exec()ing works, that the breakpoint is hit, and that the exec() is detected.
#

# Some machines have very slow exec(), and this exec()s five thousand times.
# @@timeout: 200

test/triggers/libproc-execing-bkpts breakdance test/triggers/libproc-execing-bkpts-victim 5000
