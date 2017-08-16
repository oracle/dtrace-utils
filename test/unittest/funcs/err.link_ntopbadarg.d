/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	this->buf_eth = (uint64_t *)alloca(sizeof (uint64_t));

	/* Ethernet MAC address 00:01:02:03:04:05 */
	*(this->buf_eth) = htonll(0x000102030405 << 16);
	printf("%s\n", link_ntop(-1, this->buf_eth));

	exit(0);
}

ERROR
{
	exit(1);
}
