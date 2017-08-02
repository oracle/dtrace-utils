/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Test the empty declaration of a translator
 *
 * SECTION: Translators/ Translator Declarations
 * SECTION: Translators/ Translate Operator
 *
 */

#pragma D option quiet

struct input_struct {
	int i;
	char c;
} *uvar;

struct output_struct {
	int myi;
	char myc;
};

translator struct output_struct < struct input_struct uvar >
{
};

struct input_struct in;
struct output_struct ou;

BEGIN
{
	in.i = 10;
	in.c = 'c';

	ou = xlate < struct output_struct > (in);

	printf("ou.myi: %d\tou.myc: %c\n", ou.myi, ou.myc);
}

BEGIN
/(0 != ou.myi) || (0 != ou.myc)/
{
	printf("Failed\n");
	exit(1);
}

BEGIN
/(0 == ou.myi) || (0 == ou.myc)/
{
	printf("Passed\n");
	exit(0);
}
