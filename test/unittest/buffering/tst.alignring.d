/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Positive test for ring buffer policy.
 *
 * SECTION: Buffers and Buffering/ring Policy;
 *	Buffers and Buffering/Buffer Sizes;
 *	Options and Tunables/bufsize;
 *	Options and Tunables/bufpolicy
 */

/* Incurs swapping, so
 *  @@timeout: 15
 */

#pragma D option bufpolicy=ring
#pragma D option bufsize=4k

profile:::profile-1009hz
{
	printf("%x %x\n", (int)0xaaaa, (int)0xbbbb);
}

profile:::profile-1237hz
{
	printf("%x %x %x %x %x %x\n",
	    (int)0xcccc,
	    (int)0xdddd,
	    (int)0xeeee,
	    (int)0xffff,
	    (int)0xabab,
	    (int)0xacac);
	printf("%x %x\n",
	    (uint64_t)0xaabbaabbaabbaabb,
	    (int)0xadad);
}

profile:::profile-1789hz
{
	printf("%x %x %x %x %x\n",
	    (int)0xaeae,
	    (int)0xafaf,
	    (unsigned char)0xaa,
	    (int)0xbcbc,
	    (int)0xbdbd);
}

profile-1543hz
{}

profile-1361hz
{}

tick-1sec
/i++ >= 10/
{
	exit(0);
}
