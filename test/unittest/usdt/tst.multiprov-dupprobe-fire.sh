#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -c `pwd`/test/triggers/usdt-tst-multiprov-dupprobe -qn '
prov*::: { printf("%s:%s:%s:%s\n", probeprov, probemod, probefunc, probename); }
prova$target:::entrye { printf("%s: %d %s\n", probeprov, arg0, stringof(arg1)); }
provb$target:::entryc { printf("%s: %s %d\n", probeprov, stringof(arg0), arg1); }
provc$target:::entrye { printf("%s: %s %d\n", probeprov, stringof(arg0), arg1); }
'
