/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <unistd.h>

volatile double c = 1.2;

int
main(int argc, char **argv)
{
	double a = 1.56;
	double b = (double)argc;

	for (;;) {
		c *= a;
		c += b;
		usleep(1000);
	}

	return 0;
}
