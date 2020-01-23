/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet

BEGIN
{
	j = 0;
}

tick-1ms
/i < 100/
{
	i++;
	@a[i] = sum(i);
	@b[i] = sum((25 + i) % 100);
	@c[i] = sum((50 + i) % 100);
	@d[i] = sum((75 + i) % 100);
}

tick-1ms
/i == 100 && j < 10/
{
	printf("Sorted at position %d:\n", j);
	setopt("aggsortpos", lltostr(j));
	printa("%9d %@9d %@9d %@9d %@9d %@9d %@9d\n", @a, @b, @c, @a, @d, @a);
	printf("\n");
	j++;
}

tick-1ms
/i == 100 && j == 10/
{
	exit(0);
}
