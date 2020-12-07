#!/usr/bin/gawk -f
# Oracle Linux DTrace.
# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

/^dtrace: script .* matched [0-9]* probes/ { $(NF-1) = "NNN" }
{print}
