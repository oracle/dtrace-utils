/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: -C */

/*
 * ASSERTION: Test network byte-ordering routines.
 */

#include <endian.h>

BEGIN
{
	before[0] = 0x1122LL;
	before[1] = 0x11223344LL;
	before[2] = 0x1122334455667788LL;

#if __BYTE_ORDER == __LITTLE_ENDIAN
	after[0] = 0x2211LL;
	after[1] = 0x44332211LL;
	after[2] = 0x8877665544332211LL;
#else
	after[0] = 0x1122LL;
	after[1] = 0x11223344LL;
	after[2] = 0x1122334455667788LL;
#endif
}

BEGIN
/after[0] != htons(before[0])/
{
	printf("%x rather than %x", htons(before[0]), after[0]);
	exit(1);
}

BEGIN
/after[0] != ntohs(before[0])/
{
	printf("%x rather than %x", ntohs(before[0]), after[0]);
	exit(1);
}

BEGIN
/after[1] != htonl(before[1])/
{
	printf("%x rather than %x", htonl(before[1]), after[1]);
	exit(1);
}

BEGIN
/after[1] != ntohl(before[1])/
{
	printf("%x rather than %x", ntohl(before[1]), after[1]);
	exit(1);
}

BEGIN
/after[2] != htonll(before[2])/
{
	printf("%x rather than %x", htonll(before[2]), after[2]);
	exit(1);
}

BEGIN
/after[2] != ntohll(before[2])/
{
	printf("%x rather than %x", ntohll(before[2]), after[2]);
	exit(1);
}

BEGIN
{
	exit(0);
}
