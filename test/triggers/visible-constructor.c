/*
 * Oracle Linux DTrace.
 * Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdio.h>

/* A trigger with a constructor that all can see running. */

__attribute__((__constructor__)) 
static void
at_start(void)
{
	printf("Constructor run.\n");
	fflush(stdout);
}

int
main (void)
{
	printf("main() run.\n");
	fflush(stdout);
	return 0;
}
