/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Without -Z or -xzdefs, a probe description not matching any
 *	      probes is an error.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

BEGIN,
fake:oracle:probe
{
	exit(0);
}
