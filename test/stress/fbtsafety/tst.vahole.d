#!/usr/sbin/dtrace -Cs

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@skip: nonportable */

/*
 * We set our buffer size absurdly low to prevent a flood of errors that we
 * don't care about.
 */
#pragma D option bufsize=16
#pragma D option bufpolicy=ring

fbt:::
{
	n++;
#ifdef __sparc
	trace(*(int *)0x8000000000000000 ^ rand());
#else
	trace(*(int *)(`kernelbase - 1));
#endif
}

dtrace:::ERROR
{
	err++;
}

tick-1sec
/sec++ == 10/
{
	exit(2);
}

END
/n == 0 || err == 0/
{
	exit(1);
}

END
/n != 0 && err != 0/
{
	exit(0);
}
