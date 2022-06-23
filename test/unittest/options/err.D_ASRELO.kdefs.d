/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -xkdefs option prohibits unresolved kernel symbols.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xerrtags -xlinkmode=dynamic -xknodefs -xkdefs */

BEGIN {
	trace((string)`linux_banner);
	exit(0);
}
