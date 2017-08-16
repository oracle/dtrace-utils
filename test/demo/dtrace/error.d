/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

BEGIN
{
	*(char *)NULL;
}

ERROR
{
	printf("Hit an error!");
	exit(0);
}
