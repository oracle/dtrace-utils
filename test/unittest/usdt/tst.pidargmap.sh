#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that USDT and pid probes that share underlying probes
# do not apply arg mappings (incorrectly) to the pid probes.

exec $(dirname $_test)/pidprobes.sh $1 t t
