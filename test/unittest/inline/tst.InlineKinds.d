/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * Test the code generation and results of the various kinds of inlines.
 * In particular, we test constant and expression-based scalar inlines,
 * associative array inlines, and inlines using translators.
 */

#pragma D option quiet

inline int i0 = 100 + 23;		/* constant-folded integer constant */
inline string i1 = probename;		/* string variable reference */
inline int i2 = pid != 0;		/* expression involving a variable */

struct s {
	int s_x;
};

translator struct s < int T > {
	s_x = T + 1;
};

inline struct s i3 = xlate < struct s > (i0);		/* translator */
inline int i4[int x, int y] = x + y;			/* associative array */
inline int i5[int x] = (xlate < struct s > (x)).s_x;	/* array by xlate */

BEGIN
{
	printf("i0 = %d\n", i0);
	printf("i1 = %s\n", i1);
	printf("i2 = %d\n", i2);

	printf("i3.s_x = %d\n", i3.s_x);
	printf("i4[10, 20] = %d\n", i4[10, 20]);
	printf("i5[123] = %d\n", i5[123]);

	exit(0);
}
