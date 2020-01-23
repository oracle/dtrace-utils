/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION: Pointers assignment simply copies over the pointer address in the
 * variable on the right hand side into the variable on the left hand side.
 *
 * SECTION: Pointers and Arrays/Pointer and Array Relationship
 *
 * NOTES:
 *
 */

#pragma D option quiet

int array[3];
int *ptr1;
int *ptr2;

BEGIN
{
	array[0] = 200;
	array[1] = 400;
	array[2] = 600;

	ptr1 = array;
	ptr2 = ptr1;

	ptr2[0] = 400;
	ptr2[1] = 800;
	ptr2[2] = 1200;

	printf("array[0]: %d\tptr2[0]: %d\n", array[0], ptr2[0]);
	printf("array[1]: %d\tptr2[1]: %d\n", array[1], ptr2[1]);
	printf("array[2]: %d\tptr2[2]: %d\n", array[2], ptr2[2]);

	exit(0);

}

END
/(array[0] != 400) || (array[1] != 800) || (array[2] != 1200)/
{
	printf("Error");
	exit(1);
}
