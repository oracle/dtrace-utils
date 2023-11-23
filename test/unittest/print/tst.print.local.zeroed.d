/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@nosort */

#pragma D option quiet

enum eh { FIRSTVAL = 1, SECONDVAL = 2, THIRDVAL= 3 };

/* DTrace will not parse nested anonymous structs/unions; also bitfields are not supported. */
struct test_struct {
	int	a;
	char	b[5];
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
	t->a = 0;
	/* initial null should prevent displaying rest of char array */
	t->b[0] = '\0';
	t->b[1] = 'e';
	t->b[2] = 's';
	t->b[3] = 't';
	t->b[4] = '\0';
	t->u.d = 0,
	t->s.e = (void *)NULL,
	t->s.f = 0,
	t->g = 1;
	t->h = FIRSTVAL;
	print(t);
	exit(0);
}
