/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Using a negative value for nspec throws a compiler error.
 *
 * SECTION: Speculative Tracing/Options and Tuning;
 * 	Options and Tunables/nspec
 */

#pragma D option quiet
#pragma D option nspec=-72

BEGIN
{
	self->speculateFlag = 0;
	self->commitFlag = 0;
	self->spec = speculation();
	printf("Speculative buffer ID: %d\n", self->spec);
	printf("This test shouldnt have compiled\n");
	exit(0);
}
