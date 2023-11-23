/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@nosort */

#pragma D option quiet

enum eh { FIRSTVAL = 0, SECONDVAL = 1, THIRDVAL = 3 };

/* DTrace will not parse nested anonymous structs/unions; also bitfields are not supported. */
struct test_struct {
	int     a;
	char    b[5];
	union {
		__u8 c;
		__u64 d;
	} u;
	struct {
		void *e;
		uint64_t f;
	} s;
	bool g;
	enum eh h;
};

BEGIN
{
	t = (struct test_struct *)alloca(sizeof (struct test_struct));
	t->a = 1;
	t->b[0] = 't';
	t->b[1] = 'e';
	t->b[2] = 's';
	t->b[3] = 't';
	t->b[4] = '\0';
	t->u.d = 123456789;
	t->s.e = (void *)0xffff9b7aca7e0000,
	t->s.f = 987654321;
	t->g = 1;
	t->h = SECONDVAL;
	print(t);
	exit(0);
}
