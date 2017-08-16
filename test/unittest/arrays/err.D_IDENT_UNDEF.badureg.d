/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Arrays declarations must have a positive constant as a
 * 	subscription.
 *
 * SECTION: User Process Tracing/uregs Array
 */

BEGIN
{
	printf("FOO = 0x%x\n", uregs[FOO]);
	exit(1);
}

