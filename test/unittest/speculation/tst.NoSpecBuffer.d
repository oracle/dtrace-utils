/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

/*
 * ASSERTION:
 * If no speculative buffer is available when speculation is called,
 * an ID of zero is returned.
 *
 * SECTION: Speculative Tracing/Creating a Speculation
 *
 */
#pragma D option quiet
#pragma D option nspec=1

BEGIN
{
	i = 0;
}

tick-100ms
/i < 2/
{
	self->spec = speculation();
	printf("Speculative buffer ID: %d\n", self->spec);
	i++;
}

tick-100ms
/i == 2/
{
	printf("i: %d\tself->spec: %d", i, self->spec);
	exit(self->spec == 0 ? 0 : 1);
}
