#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

exec $dtrace $dt_flags -lv -P 'prov*' -c `pwd`/test/triggers/usdt-tst-multiprov-dupprobe
