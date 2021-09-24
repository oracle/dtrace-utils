/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet
#pragma D option strsize=7

BEGIN
{
	printf("%s\n", lltostr(0));
	printf("%s\n", lltostr(1));
	printf("%s\n", lltostr(12));
	printf("%s\n", lltostr(123));
	printf("%s\n", lltostr(1234));
	printf("%s\n", lltostr(12345));
	printf("%s\n", lltostr(123456));
	printf("%s\n", lltostr(1234567));
	printf("%s\n", lltostr(12345678));
	printf("%s\n", lltostr(123456789));
	printf("%s\n", lltostr(1234567890));
	printf("%s\n", lltostr(-1));
	printf("%s\n", lltostr(-12));
	printf("%s\n", lltostr(-123));
	printf("%s\n", lltostr(-1234));
	printf("%s\n", lltostr(-12345));
	printf("%s\n", lltostr(-123456));
	printf("%s\n", lltostr(-1234567));
	printf("%s\n", lltostr(-12345678));
	printf("%s\n", lltostr(-123456789));
	printf("%s\n", lltostr(-1234567890));
	printf("%s\n", lltostr(9999999));
	printf("%s\n", lltostr(10000000));
	printf("%s\n", lltostr(-999999));
	printf("%s\n", lltostr(-1000000));
	exit(0);
}
