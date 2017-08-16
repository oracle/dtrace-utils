/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	str = "fooeyfooeyfoo";
	this->success = 0;

	c = 'f';
	printf("strchr(\"%s\", '%c') = \"%s\"\n", str, c, strchr(str, c));
	printf("strrchr(\"%s\", '%c') = \"%s\"\n", str, c, strrchr(str, c));

	c = 'y';
	printf("strchr(\"%s\", '%c') = \"%s\"\n", str, c, strchr(str, c));
	printf("strrchr(\"%s\", '%c') = \"%s\"\n", str, c, strrchr(str, c));

	printf("strrchr(\"%s\", '%c') = \"%s\"\n", strchr(str, c), c,
	    strrchr(strchr(str, c), c));

	this->success = 1;
}

BEGIN
/!this->success/
{
	exit(1);
}

BEGIN
/strchr(str, 'a') != NULL/
{
	exit(2);
}

BEGIN
/strrchr(str, 'a') != NULL/
{
	exit(3);
}

BEGIN
{
	exit(0);
}
