/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -xzdefs option allows probe definitions that match no probes.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xzdefs */

BEGIN,
fake:oracle:probe
{
	exit(0);
}
