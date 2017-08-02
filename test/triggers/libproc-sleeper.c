/*
 * A process that sleeps for a while.
 */

/*
 * Oracle Linux DTrace.
 * Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <unistd.h>
#include <stdio.h>

int main(void)
{
	sleep(10);
	fprintf(stderr, "Exiting.\n");
	return 0;
}
