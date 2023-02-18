/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -xversion option works.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xversion=99.1 */
BEGIN {
	exit(0);
}
