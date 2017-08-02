/*
 * Oracle Linux DTrace.
 * Copyright © 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: depends on ip */

#pragma D option quiet

ipaddr_t *ip4a;
ipaddr_t *ip4b;
ipaddr_t *ip4c;
ipaddr_t *ip4d;
struct in6_addr *ip6a;
struct in6_addr *ip6b;
struct in6_addr *ip6c;
struct in6_addr *ip6d;
struct in6_addr *ip6e;
struct in6_addr *ip6f;
struct in6_addr *ip6g;
struct in6_addr *ip6h;

BEGIN
{
	this->buf4a = alloca(sizeof (ipaddr_t));
	this->buf4b = alloca(sizeof (ipaddr_t));
	this->buf4c = alloca(sizeof (ipaddr_t));
	this->buf4d = alloca(sizeof (ipaddr_t));
	this->buf6a = alloca(sizeof (struct in6_addr));
	this->buf6b = alloca(sizeof (struct in6_addr));
	this->buf6c = alloca(sizeof (struct in6_addr));
	this->buf6d = alloca(sizeof (struct in6_addr));
	this->buf6e = alloca(sizeof (struct in6_addr));
	this->buf6f = alloca(sizeof (struct in6_addr));
	this->buf6g = alloca(sizeof (struct in6_addr));
	this->buf6h = alloca(sizeof (struct in6_addr));
	ip4a = this->buf4a;
	ip4b = this->buf4b;
	ip4c = this->buf4c;
	ip4d = this->buf4d;
	ip6a = this->buf6a;
	ip6b = this->buf6b;
	ip6c = this->buf6c;
	ip6d = this->buf6d;
	ip6e = this->buf6e;
	ip6f = this->buf6f;
	ip6g = this->buf6g;
	ip6h = this->buf6h;

	*ip4a = htonl(0xc0a80117);
	*ip4b = htonl(0x7f000001);
	*ip4c = htonl(0xffffffff);
	*ip4d = htonl(0x00000000);
	ip6a->_S6_un._S6_u8[0] = 0xfe;
	ip6a->_S6_un._S6_u8[1] = 0x80;
	ip6a->_S6_un._S6_u8[8] = 0x02;
	ip6a->_S6_un._S6_u8[9] = 0x14;
	ip6a->_S6_un._S6_u8[10] = 0x4f;
	ip6a->_S6_un._S6_u8[11] = 0xff;
	ip6a->_S6_un._S6_u8[12] = 0xfe;
	ip6a->_S6_un._S6_u8[13] = 0x0b;
	ip6a->_S6_un._S6_u8[14] = 0x76;
	ip6a->_S6_un._S6_u8[15] = 0xc8;
	ip6b->_S6_un._S6_u8[0] = 0x10;
	ip6b->_S6_un._S6_u8[1] = 0x80;
	ip6b->_S6_un._S6_u8[10] = 0x08;
	ip6b->_S6_un._S6_u8[11] = 0x08;
	ip6b->_S6_un._S6_u8[13] = 0x20;
	ip6b->_S6_un._S6_u8[13] = 0x0c;
	ip6b->_S6_un._S6_u8[14] = 0x41;
	ip6b->_S6_un._S6_u8[15] = 0x7a;
	ip6c->_S6_un._S6_u8[15] = 0x01;
	ip6e->_S6_un._S6_u8[12] = 0x7f;
	ip6e->_S6_un._S6_u8[15] = 0x01;
	ip6f->_S6_un._S6_u8[10] = 0xff;
	ip6f->_S6_un._S6_u8[11] = 0xff;
	ip6f->_S6_un._S6_u8[12] = 0x7f;
	ip6f->_S6_un._S6_u8[15] = 0x01;
	ip6g->_S6_un._S6_u8[10] = 0xff;
	ip6g->_S6_un._S6_u8[11] = 0xfe;
	ip6g->_S6_un._S6_u8[12] = 0x7f;
	ip6g->_S6_un._S6_u8[15] = 0x01;
	ip6h->_S6_un._S6_u8[0] = 0xff;
	ip6h->_S6_un._S6_u8[1] = 0xff;
	ip6h->_S6_un._S6_u8[2] = 0xff;
	ip6h->_S6_un._S6_u8[3] = 0xff;
	ip6h->_S6_un._S6_u8[4] = 0xff;
	ip6h->_S6_un._S6_u8[5] = 0xff;
	ip6h->_S6_un._S6_u8[6] = 0xff;
	ip6h->_S6_un._S6_u8[7] = 0xff;
	ip6h->_S6_un._S6_u8[8] = 0xff;
	ip6h->_S6_un._S6_u8[9] = 0xff;
	ip6h->_S6_un._S6_u8[10] = 0xff;
	ip6h->_S6_un._S6_u8[11] = 0xff;
	ip6h->_S6_un._S6_u8[12] = 0xff;
	ip6h->_S6_un._S6_u8[13] = 0xff;
	ip6h->_S6_un._S6_u8[14] = 0xff;
	ip6h->_S6_un._S6_u8[15] = 0xff;

	printf("%s\n", inet_ntop(AF_INET, ip4a));
	printf("%s\n", inet_ntop(AF_INET, ip4b));
	printf("%s\n", inet_ntop(AF_INET, ip4c));
	printf("%s\n", inet_ntop(AF_INET, ip4d));
	printf("%s\n", inet_ntop(AF_INET6, ip6a));
	printf("%s\n", inet_ntop(AF_INET6, ip6b));
	printf("%s\n", inet_ntop(AF_INET6, ip6c));
	printf("%s\n", inet_ntop(AF_INET6, ip6d));
	printf("%s\n", inet_ntop(AF_INET6, ip6e));
	printf("%s\n", inet_ntop(AF_INET6, ip6f));
	printf("%s\n", inet_ntop(AF_INET6, ip6g));
	printf("%s\n", inet_ntop(AF_INET6, ip6h));

	exit(0);
}
