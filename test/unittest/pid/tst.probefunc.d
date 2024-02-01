/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: -c /bin/date */

pid$target:libc.so:malloc:entry
{
	trace(probefunc);
}

pid$target:libc.so:malloc:entry
/probefunc == "malloc"/
{
	exit(0);
}

pid$target:libc.so:malloc:entry
{
	exit(1);
}
