/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * 	trace with NULL argument - generate a bunch of errors
 *
 * SECTION: Options and Tunables/bufsize;
 * 	Options and Tunables/bufpolicy
 */

/*
 * We set our buffer size absurdly low to prevent a flood of errors that we
 * don't care about.
 */

#pragma D option bufsize=16
#pragma D option bufpolicy=ring

fbt:::
{
	on = (timestamp / 1000000000) & 1;
}

fbt:::
/on/
{
	n++;
	trace(*(int *)NULL);
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
