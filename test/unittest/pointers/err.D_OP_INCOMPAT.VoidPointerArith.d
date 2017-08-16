/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * May not apply pointer arithmetic to an object of type  void *
 *
 * SECTION: Pointers and Arrays/Generic Pointers
 */

#pragma D option quiet

void *p;

BEGIN
{
	printf("p: %d\n", (int) p);
	printf("p+1: %d\n", (int) (p+1));
	printf("p+2: %d\n", (int) (p+2));
	exit(0);
}
