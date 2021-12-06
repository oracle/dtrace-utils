/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	x = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	y = "abcABC";
	printf("%s\n", strtok(x, y));
	y = "MNO";
	printf("%s\n", strtok(NULL, y));
	printf("%s\n", strtok(NULL, y));
	y = "0123456789";
	printf("%s\n", strtok(NULL, y));
	printf("%s\n", strtok(NULL, y) == NULL ? "got expected NULL" : "ERROR: non-NULL");

	x = "";
	y = "";
	printf("%s\n", strtok(x, y) == NULL ? "got expected NULL" : "ERROR: non-NULL");

	x = "foo";
	printf("%s\n", strtok(x, y));

	x = "";
	y = "foo";
	printf("%s\n", strtok(x, y) == NULL ? "got expected NULL" : "ERROR: non-NULL");

	exit(0);
}

ERROR
{
	exit(1);
}
