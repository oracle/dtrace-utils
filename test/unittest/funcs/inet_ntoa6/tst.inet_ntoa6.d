/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

struct in6_addr *ip6a;
struct in6_addr *ip6b;
struct in6_addr *ip6c;
struct in6_addr *ip6d;
struct in6_addr *ip6e;
struct in6_addr *ip6f;
struct in6_addr *ip6g;
struct in6_addr *ip6h;
struct in6_addr *ip6i;
struct in6_addr *ip6j;
struct in6_addr *ip6k;

BEGIN
{
	this->buf6a = alloca(sizeof(struct in6_addr));
	this->buf6b = alloca(sizeof(struct in6_addr));
	this->buf6c = alloca(sizeof(struct in6_addr));
	this->buf6d = alloca(sizeof(struct in6_addr));
	this->buf6e = alloca(sizeof(struct in6_addr));
	this->buf6f = alloca(sizeof(struct in6_addr));
	this->buf6g = alloca(sizeof(struct in6_addr));
	this->buf6h = alloca(sizeof(struct in6_addr));
	this->buf6i = alloca(sizeof(struct in6_addr));
	this->buf6j = alloca(sizeof(struct in6_addr));
	this->buf6k = alloca(sizeof(struct in6_addr));
	ip6a = this->buf6a;
	ip6b = this->buf6b;
	ip6c = this->buf6c;
	ip6d = this->buf6d;
	ip6e = this->buf6e;
	ip6f = this->buf6f;
	ip6g = this->buf6g;
	ip6h = this->buf6h;
	ip6i = this->buf6i;
	ip6j = this->buf6j;
	ip6k = this->buf6k;

	ip6a->in6_u.u6_addr8[0] = 0xfe;
	ip6a->in6_u.u6_addr8[1] = 0x80;
	ip6a->in6_u.u6_addr8[8] = 0x02;
	ip6a->in6_u.u6_addr8[9] = 0x14;
	ip6a->in6_u.u6_addr8[10] = 0x4f;
	ip6a->in6_u.u6_addr8[11] = 0xff;
	ip6a->in6_u.u6_addr8[12] = 0xfe;
	ip6a->in6_u.u6_addr8[13] = 0x0b;
	ip6a->in6_u.u6_addr8[14] = 0x76;
	ip6a->in6_u.u6_addr8[15] = 0xc8;
	ip6b->in6_u.u6_addr8[0] = 0x10;
	ip6b->in6_u.u6_addr8[1] = 0x80;
	ip6b->in6_u.u6_addr8[10] = 0x08;
	ip6b->in6_u.u6_addr8[11] = 0x08;
	ip6b->in6_u.u6_addr8[13] = 0x20;
	ip6b->in6_u.u6_addr8[13] = 0x0c;
	ip6b->in6_u.u6_addr8[14] = 0x41;
	ip6b->in6_u.u6_addr8[15] = 0x7a;
	ip6c->in6_u.u6_addr8[15] = 0x01;
	ip6e->in6_u.u6_addr8[12] = 0x7f;
	ip6e->in6_u.u6_addr8[15] = 0x01;
	ip6f->in6_u.u6_addr8[10] = 0xff;
	ip6f->in6_u.u6_addr8[11] = 0xff;
	ip6f->in6_u.u6_addr8[12] = 0x7f;
	ip6f->in6_u.u6_addr8[15] = 0x01;
	ip6g->in6_u.u6_addr8[10] = 0xff;
	ip6g->in6_u.u6_addr8[11] = 0xfe;
	ip6g->in6_u.u6_addr8[12] = 0x7f;
	ip6g->in6_u.u6_addr8[15] = 0x01;
	ip6h->in6_u.u6_addr8[0] = 0xfe;
	ip6h->in6_u.u6_addr8[1] = 0x12;
	ip6h->in6_u.u6_addr8[2] = 0x34;
	ip6h->in6_u.u6_addr8[3] = 0x56;
	ip6h->in6_u.u6_addr8[4] = 0x78;
	ip6h->in6_u.u6_addr8[5] = 0x9a;
	ip6h->in6_u.u6_addr8[6] = 0xbc;
	ip6h->in6_u.u6_addr8[7] = 0xde;
	ip6h->in6_u.u6_addr8[8] = 0x02;
	ip6h->in6_u.u6_addr8[9] = 0x14;
	ip6i->in6_u.u6_addr8[0] = 0xfe;
	ip6i->in6_u.u6_addr8[1] = 0x12;
	ip6i->in6_u.u6_addr8[2] = 0x34;
	ip6i->in6_u.u6_addr8[3] = 0x56;
	ip6i->in6_u.u6_addr8[4] = 0x78;
	ip6i->in6_u.u6_addr8[5] = 0x9a;
	ip6i->in6_u.u6_addr8[6] = 0xbc;
	ip6i->in6_u.u6_addr8[7] = 0xde;
	ip6i->in6_u.u6_addr8[8] = 0x02;
	ip6i->in6_u.u6_addr8[9] = 0x14;
	ip6i->in6_u.u6_addr8[10] = 0x4f;
	ip6i->in6_u.u6_addr8[11] = 0xff;
	ip6j->in6_u.u6_addr8[0] = 0xfe;
	ip6j->in6_u.u6_addr8[1] = 0x12;
	ip6j->in6_u.u6_addr8[2] = 0x34;
	ip6j->in6_u.u6_addr8[3] = 0x56;
	ip6j->in6_u.u6_addr8[4] = 0x78;
	ip6j->in6_u.u6_addr8[5] = 0x9a;
	ip6j->in6_u.u6_addr8[6] = 0xbc;
	ip6j->in6_u.u6_addr8[7] = 0xde;
	ip6j->in6_u.u6_addr8[8] = 0x02;
	ip6j->in6_u.u6_addr8[9] = 0x14;
	ip6j->in6_u.u6_addr8[10] = 0x4f;
	ip6j->in6_u.u6_addr8[11] = 0xff;
	ip6j->in6_u.u6_addr8[12] = 0xfe;
	ip6j->in6_u.u6_addr8[13] = 0x0b;
	ip6k->in6_u.u6_addr8[0] = 0xfe;
	ip6k->in6_u.u6_addr8[1] = 0x12;
	ip6k->in6_u.u6_addr8[2] = 0x34;
	ip6k->in6_u.u6_addr8[3] = 0x56;
	ip6k->in6_u.u6_addr8[4] = 0x78;
	ip6k->in6_u.u6_addr8[5] = 0x9a;
	ip6k->in6_u.u6_addr8[6] = 0xbc;
	ip6k->in6_u.u6_addr8[7] = 0xde;
	ip6k->in6_u.u6_addr8[8] = 0x02;
	ip6k->in6_u.u6_addr8[9] = 0x14;
	ip6k->in6_u.u6_addr8[10] = 0x4f;
	ip6k->in6_u.u6_addr8[11] = 0xff;
	ip6k->in6_u.u6_addr8[12] = 0xfe;
	ip6k->in6_u.u6_addr8[13] = 0x0b;
	ip6k->in6_u.u6_addr8[14] = 0x76;
	ip6k->in6_u.u6_addr8[15] = 0xc8;

	printf("%s\n", inet_ntoa6(ip6a));
	printf("%s\n", inet_ntoa6(ip6b));
	printf("%s\n", inet_ntoa6(ip6c));
	printf("%s\n", inet_ntoa6(ip6d));
	printf("%s\n", inet_ntoa6(ip6e));
	printf("%s\n", inet_ntoa6(ip6f));
	printf("%s\n", inet_ntoa6(ip6g));
	printf("%s\n", inet_ntoa6(ip6h));
	printf("%s\n", inet_ntoa6(ip6i));
	printf("%s\n", inet_ntoa6(ip6j));
	printf("%s\n", inet_ntoa6(ip6k));

	exit(0);
}
