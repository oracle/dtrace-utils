/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option destructive
#pragma D option jstackstrsize=1
#pragma D option quiet

BEGIN
{
	system("java -version");
}

syscall:::entry
/progenyof($pid)/
{
	@[jstack()] = count();
}

proc:::exit
/progenyof($pid) && execname == "java"/
{
	exit(0);
}

END
{
	printa("\r", @);
	printf("\n");
}
