#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#/

dtrace=$1

$dtrace $dt_flags -qSn '
BEGIN
{
	a = 100000000;
	b = 20000000;
	c = 3000000;
	d = 400000;
	e = 50000;
	f = 6000;
	g = 700;
	h = 80;
	i = 9;
	trace(++a + (++b + (++c + (++d + (++e + (++f + (++g + (++h + ++i))))))));
	exit(0);
}' 2>&1 | gawk '/! (spill|restore)/ { sub(/^[^:]+: /, ""); print; next; }
	       { s = $0; }
	       END { print s; }'
