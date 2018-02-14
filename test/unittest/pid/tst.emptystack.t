#!/usr/bin/awk -f
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
BEGIN {
	for (;;) {
		fn = "/etc/hosts";

		while ((getline < fn) == 1)
			count++;

		close(fn);
	}
}
