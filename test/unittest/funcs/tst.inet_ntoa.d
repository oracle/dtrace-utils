/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

ipaddr_t *ip4a;
ipaddr_t *ip4b;
ipaddr_t *ip4c;
ipaddr_t *ip4d;

BEGIN
{
	this->buf4a = alloca(sizeof(ipaddr_t));
	this->buf4b = alloca(sizeof(ipaddr_t));
	this->buf4c = alloca(sizeof(ipaddr_t));
	this->buf4d = alloca(sizeof(ipaddr_t));
	ip4a = this->buf4a;
	ip4b = this->buf4b;
	ip4c = this->buf4c;
	ip4d = this->buf4d;

	*ip4a = htonl(0xc0a80117);
	*ip4b = htonl(0x7f000001);
	*ip4c = htonl(0xffffffff);
	*ip4d = htonl(0x00000000);

	printf("%s\n", inet_ntoa(ip4a));
	printf("%s\n", inet_ntoa(ip4b));
	printf("%s\n", inet_ntoa(ip4c));
	printf("%s\n", inet_ntoa(ip4d));

	exit(0);
}
