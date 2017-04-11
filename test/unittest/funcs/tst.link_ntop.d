/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2017 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

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
