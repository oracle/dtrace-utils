#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This script tests that name lookups in the main program, in libraries, of
# interposed functions and in other lmids all works, and resolves the right
# symbol.
#

export LD_BIND_NOW=t

test/triggers/libproc-lookup-by-name main_func 0 test/triggers/libproc-lookup-victim 0
test/triggers/libproc-lookup-by-name lib_func 0 test/triggers/libproc-lookup-victim 1
test/triggers/libproc-lookup-by-name interposed_func 0 test/triggers/libproc-lookup-victim 2
test/triggers/libproc-lookup-by-name lib_func 1 test/triggers/libproc-lookup-victim 3
