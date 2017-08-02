/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
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
	exit(0);
}
