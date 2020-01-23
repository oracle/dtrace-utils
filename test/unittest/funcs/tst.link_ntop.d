/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet

uint8_t ibaddr[20];

BEGIN
{
	this->buf_eth = (uint64_t *)alloca(sizeof (uint64_t));

	/* Ethernet MAC address 00:01:02:03:04:05 */
	*(this->buf_eth) = htonll(0x000102030405 << 16);
	printf("%s\n", link_ntop(ARPHRD_ETHER, this->buf_eth));

	/* Ethernet MAC address ff:ff:ff:ff:ff:ff */
	*(this->buf_eth) = htonll(0xffffffffffff << 16);
	printf("%s\n", link_ntop(ARPHRD_ETHER, this->buf_eth));

	/* Ethernet MAC address 01:33:0a:00:00:01 */
	*(this->buf_eth) = htonll(0x01330a000001 << 16);
	printf("%s\n", link_ntop(ARPHRD_ETHER, this->buf_eth));

	/* Ethernet MAC address 00:10:e0:8a:71:5e */
	*(this->buf_eth) = htonll(0x0010e08a715e << 16);
	printf("%s\n", link_ntop(ARPHRD_ETHER, this->buf_eth));

	/* Ethernet MAC address 2:8:20:ac:4:e */
	*(this->buf_eth) = htonll(0x020820ac040e << 16);
	printf("%s\n", link_ntop(ARPHRD_ETHER, this->buf_eth));

	ibaddr[0] = 0x10;
	ibaddr[1] = 0x80;
	ibaddr[2] = 0x08;
	ibaddr[3] = 0x08;
	ibaddr[4] = 0x20;
	ibaddr[5] = 0x0c;
	ibaddr[6] = 0x41;
	ibaddr[7] = 0x7a;
	ibaddr[8] = 0x01;
	ibaddr[9] = 0x7f;
	ibaddr[10] = 0x01;
	ibaddr[11] = 0xff;
	ibaddr[12] = 0xff;
	ibaddr[13] = 0x7f;
	ibaddr[14] = 0x01;
	ibaddr[15] = 0xff;
	ibaddr[16] = 0xfe;
	ibaddr[17] = 0x7f;
	ibaddr[18] = 0x01;
	ibaddr[19] = 0xff;
	printf("%s\n", link_ntop(ARPHRD_INFINIBAND, ibaddr));

	ibaddr[0] = 0x80;
	ibaddr[1] = 0x00;
	ibaddr[2] = 0x04;
	ibaddr[3] = 0x04;
	ibaddr[4] = 0xFE;
	ibaddr[5] = 0x8C;
	ibaddr[6] = 0x41;
	ibaddr[7] = 0x7A;
	ibaddr[8] = 0x00;
	ibaddr[9] = 0x00;
	ibaddr[10] = 0x00;
	ibaddr[11] = 0x00;
	ibaddr[12] = 0x00;
	ibaddr[13] = 0x02;
	ibaddr[14] = 0xC9;
	ibaddr[15] = 0x02;
	ibaddr[16] = 0x00;
	ibaddr[17] = 0x29;
	ibaddr[18] = 0x31;
	ibaddr[19] = 0xCD;
	printf("%s\n", link_ntop(ARPHRD_INFINIBAND, ibaddr));

	exit(0);
}
