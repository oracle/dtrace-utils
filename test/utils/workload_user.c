/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdlib.h>

/*
 * The command-line argument specifies how many iterations to execute
 * in this user-space-intensive loop to run.
 */

int
main(int argc, const char **argv)
{
	double x = 0.;
	long long n;

	if (argc < 2)
		return 1;
	n = atoll(argv[1]);

	for (long long i = 0; i < n; i++)
		x = 0.5 * x + 1.;

	/*
	 * Check the result (albeit simplistically)
	 * so compiler won't optimize away the loop.
	 */
	return (x > 5.) ? 1 : 0;
}
