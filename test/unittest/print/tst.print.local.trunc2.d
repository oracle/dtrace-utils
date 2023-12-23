/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@nosort */

#pragma D option quiet

struct test_struct {
	long long	x;
	struct {
		long long x;
		struct {
			long long x;
			struct {
				long long x;
				struct {
					long long x;
					struct {
						long long x;
						struct {
							long long x;
							struct {
								long long x;
							} g;
						} f;
					} e;
				} d;
			} c;
		} b;
	} a;
};

BEGIN
{
	t = (struct test_struct *)alloca(sizeof (struct test_struct));
	t->x = 1111111111111111ll;
	t->a.x = 2222222222222222ll;
	t->a.b.x = 3333333333333333ll;
	t->a.b.c.x = 4444444444444444ll;
	t->a.b.c.d.x = 5555555555555555ll;
	t->a.b.c.d.e.x = 6666666666666666ll;
	t->a.b.c.d.e.f.x = 7777777777777777ll;
	t->a.b.c.d.e.f.g.x = 8888888888888888ll;

	setopt("printsize", "90"); print(t);
	setopt("printsize", "64"); print(t);
	setopt("printsize", "56"); print(t);
	setopt("printsize", "48"); print(t);
	setopt("printsize", "40"); print(t);
	setopt("printsize", "32"); print(t);
	exit(0);
}
