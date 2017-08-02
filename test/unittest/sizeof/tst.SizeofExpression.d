/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Test the use of sizeof with expressions.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 */

#pragma D option quiet

BEGIN
{
	printf("sizeof ('c') : %d\n", sizeof ('c'));
	printf("sizeof (10 * 'c') : %d\n", sizeof (10 * 'c'));
	printf("sizeof (100 + 12345) : %d\n", sizeof (100 + 12345));
	printf("sizeof (1234567890) : %d\n", sizeof (1234567890));

	printf("sizeof (1234512345 * 1234512345 * 12345678 * 1ULL) : %d\n",
	sizeof (1234512345 * 1234512345 * 12345678 * 1ULL));
	printf("sizeof (-129) : %d\n", sizeof (-129));
	printf("sizeof (0x67890/0x77000) : %d\n", sizeof (0x67890/0x77000));

	printf("sizeof (3 > 2 ? 3 : 2) : %d\n", sizeof (3 > 2 ? 3 : 2));

	exit(0);
}

END
/(4 != sizeof ('c')) || (4 != sizeof (10 * 'c')) ||
    (4 != sizeof (100 + 12345)) || (4 != sizeof (1234567890)) ||
    (8 != sizeof (1234512345 * 1234512345 * 12345678 * 1ULL)) ||
    (4 != sizeof (-129)) || (4 != sizeof (0x67890/0x77000)) ||
    (4 != sizeof (3 > 2 ? 3 : 2))/
{
	exit(1);
}
