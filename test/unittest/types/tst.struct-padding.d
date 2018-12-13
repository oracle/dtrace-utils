/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Statically verify that structure padding is correct by checking the
 *   values of statically-inserted structure array elements.
 *
 * SECTION: Structs and Unions/Structs
 */

#pragma D option quiet

BEGIN
{
	printf("int = %i/%i, %i/%i\n",
	       dt_test`intish[0].foo, dt_test`intish[0].bar,
	       dt_test`intish[1].foo, dt_test`intish[1].bar);
	printf("long = %li/%i, %li/%i\n",
	       dt_test`longish[0].foo, dt_test`longish[0].bar,
	       dt_test`longish[1].foo, dt_test`longish[1].bar);
	printf("long long = %lli/%li, %lli/%li\n",
	       dt_test`longlongish[0].foo, dt_test`longlongish[0].bar,
	       dt_test`longlongish[1].foo, dt_test`longlongish[1].bar);
	printf("scatter_failure = %i/%i/%i/%i/%i, %i/%i/%i/%i/%i\n",
	       dt_test`scatter_failure[0].a, dt_test`scatter_failure[0].b,
	       dt_test`scatter_failure[0].c, dt_test`scatter_failure[0].d,
	       dt_test`scatter_failure[0].e, dt_test`scatter_failure[1].a,
	       dt_test`scatter_failure[1].b, dt_test`scatter_failure[1].c,
	       dt_test`scatter_failure[1].d, dt_test`scatter_failure[1].e);

	exit(0);
}
