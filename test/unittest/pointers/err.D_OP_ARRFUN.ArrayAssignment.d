/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Arrays may not be assigned as a whole in D.
 *
 * SECTION: Pointers and Arrays/Pointer and Array Relationship
 *
 * NOTES:
 *
 */

#pragma D option quiet

int array1[3];
int array2[3];

BEGIN
{
	array1[0] = 200;
	array1[1] = 400;
	array1[2] = 600;

	array2[0] = 300;
	array2[1] = 500;
	array2[2] = 700;

	array2 = array1;

	printf("array1[0]: %d\tarray2[0]: %d\n", array1[0], array2[0]);
	printf("array1[1]: %d\tarray2[1]: %d\n", array1[1], array2[1]);
	printf("array1[2]: %d\tarray2[2]: %d\n", array1[2], array2[2]);

	exit(0);

}
