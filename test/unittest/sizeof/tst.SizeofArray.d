/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: sizeof returns the size in bytes of any D expression or data
 * type. For a simpler array, the sizeof on the array variable itself gives
 * the sum total of memory allocated to the array in bytes. With individual
 * members of the array it gives their respective sizes.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 */
#pragma D option quiet

int array[5];

BEGIN
{
	array[0] = 010;
	array[1] = 100;
	array[2] = 210;

	printf("sizeof (array): %d\n", sizeof (array));
	printf("sizeof (array[0]): %d\n", sizeof (array[0]));
	printf("sizeof (array[1]): %d\n", sizeof (array[1]));
	printf("sizeof (array[2]): %d\n", sizeof (array[2]));

	exit(0);
}

END
/(20 != sizeof (array)) || (4 != sizeof (array[0])) || (4 != sizeof (array[1]))
    || (4 != sizeof (array[2]))/
{
	exit(1);
}
