/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@nosort */

#pragma D option quiet
#pragma D option printsize=8

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
	t->a = 1;
	t->b[0] = 't';
	t->b[1] = 'e';
	t->b[2] = 's';
	t->b[3] = 't';
	t->b[4] = '\0';
	t->u.d = 12345678,
	t->s.e = (void *)0xfeedfacefeedface;
	t->s.f = 100,
	t->g = 1;
	t->h = FIRSTVAL;
	printf("a %d; b %d; u %d; s %d; g %d; h %d\n",
	    offsetof(struct test_struct, a),
	    offsetof(struct test_struct, b),
	    offsetof(struct test_struct, u),
	    offsetof(struct test_struct, s),
	    offsetof(struct test_struct, g),
	    offsetof(struct test_struct, h));
	print(t);
	/* ensure printsize can be dynamically changed */
	setopt("printsize", "24");
	print(t);
	exit(0);
}
