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
	trace(htons(0x1122));
}

fbt:::
/on/
{
	trace(htonl(0x11223344));
}

fbt:::
/on/
{
	trace(htonll(0x1122334455667788));
}

fbt:::
/on/
{
	trace(ntohs(0x1122));
}

fbt:::
/on/
{
	trace(ntohl(0x11223344));
}

fbt:::
/on/
{
	trace(ntohll(0x1122334455667788));
}

tick-1sec
/n++ == 10/
{
	exit(0);
}
