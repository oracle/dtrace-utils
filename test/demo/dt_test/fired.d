/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@trigger: testprobe */

BEGIN
{
	start=0;
}

dt_test:::test
{
	start++;
	trace(start);
}

syscall::exit_group:entry
/execname == "testprobe"/
{
	exit(0);
}

END
{
	printf("test probe is fired: %d\n", start);
}
