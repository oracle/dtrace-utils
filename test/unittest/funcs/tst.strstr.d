/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet

BEGIN
{
	str = "foobarbarbazbarbop";
	this->success = 0;

	c = str;
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	c = "baz";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	c = "bar";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	c = "bazbarbop";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	c = "barba";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	printf("strstr(\"%s\", \"%s\") = \"%s\"\n",
	    strstr(str, "baz"), strstr(str, "zba"),
	    strstr(strstr(str, "baz"), strstr(str, "zba")));

	c = "";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	str = "";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	str = "f";
	c = "f";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	this->success = 1;
}

BEGIN
/!this->success/
{
	exit(1);
}

BEGIN
/strstr(str, "barbarf") != NULL/
{
	exit(2);
}

BEGIN
/strstr(str, "foobarbarbazbarbopp") != NULL/
{
	exit(3);
}

BEGIN
{
	exit(0);
}
