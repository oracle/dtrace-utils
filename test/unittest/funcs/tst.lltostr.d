/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	printf("%s\n", lltostr(0));
	printf("%s\n", lltostr(1));
	printf("%s\n", lltostr(-1));
	printf("%s\n", lltostr(123456789));
	printf("%s\n", lltostr(-123456789));
	printf("%s\n", lltostr(1LL << 62));
	printf("%s\n", lltostr(-(1LL << 62)));
	printf("%s\n", lltostr(0x7ffffffffffffffe));
	printf("%s\n", lltostr(0x7fffffffffffffff));
	printf("%s\n", lltostr(0x8000000000000000));
	printf("%s\n", lltostr(0x8000000000000001));
	exit(0);
}
