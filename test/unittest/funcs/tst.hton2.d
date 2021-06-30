/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test network byte-ordering routines.
 */

#pragma D option quiet

BEGIN
{
	printf("%x\n", htons (0x123456789abcdef0ll));
	printf("%x\n", htonl (0x123456789abcdef0ll));
	printf("%x\n", htonll(0x123456789abcdef0ll));
	printf("%x\n", ntohs (0x123456789abcdef0ll));
	printf("%x\n", ntohl (0x123456789abcdef0ll));
	printf("%x\n", ntohll(0x123456789abcdef0ll));
	exit(0);
}

ERROR
{
	exit(1);
}
