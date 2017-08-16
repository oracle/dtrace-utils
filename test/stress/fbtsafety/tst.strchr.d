/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option bufsize=1000
#pragma D option bufpolicy=ring
#pragma D option statusrate=10ms

fbt:::
{
	on = (timestamp / 1000000000) & 1;
}

fbt:::
/on/
{
	trace(strchr((char *)(rand() ^ timestamp), rand()));
}

fbt:::
/on/
{
	trace(strrchr((char *)(rand() ^ timestamp), rand()));
}

fbt:::entry
/on/
{
	trace(strchr((char *)arg0, '!'));
}

fbt:::entry
/on/
{
	trace(strrchr((char *)arg0, '!'));
}

tick-1sec
/n++ == 10/
{
	exit(0);
}
