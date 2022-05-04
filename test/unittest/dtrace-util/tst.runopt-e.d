/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -e option surpresses execution.
 *
 * SECTION: dtrace Utility/-e Option
 */

/* @@runtest-opts: -e */

#pragma D option quiet

BEGIN
{
	trace("It executed (and should not have)!\n");
	exit(0);
}
