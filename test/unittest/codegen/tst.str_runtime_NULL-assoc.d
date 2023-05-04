/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	a1[1234] = strchr("abcdefghij", 'Z');
	a2[1234] = strstr("ABCDEFGHIJ", "a");
	a3[1234] = a1[1234];
}

BEGIN
/a1[1234] != a2[1234] || a1[1234] != a2[1234] || a1[1234] != a3[1234] || a1[1234] != NULL/
{
	printf("FAIL\n");
}

BEGIN
{
	printf("done\n");
	exit(0);
}

ERROR
{
	exit(1);
}
