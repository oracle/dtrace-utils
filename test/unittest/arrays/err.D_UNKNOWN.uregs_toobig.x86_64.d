/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:  Check the constants used to access uregs[].
 *
 * SECTION: User Process Tracing/uregs Array
 */

#pragma D option quiet

BEGIN
{
	trace(uregs[42]);
        exit(0);
}
