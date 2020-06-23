#!/usr/bin/gawk -f
# Oracle Linux DTrace.
# Copyright (c) 2016, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

/^DEPTH/ {
	depth = int($2);
}

/^TRACE BEGIN/ {
	getline;
	count = 0;
	while ($0 !~ /^TRACE END/) {
		if (getline != 1) {
			print "EOF or error while processing stack\n";
			exit 0;
		}
		if (NF)
			count++;
	}
}

END {
	if (count < depth)
		printf "Stack depth too large (%d > %d)\n", depth, count;
	else if (count > depth)
		printf "Stack depth too small (%d < %d)\n", depth, count;
	else
		printf "Stack depth OK\n";
}
