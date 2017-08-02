/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Test the declaration of a translator without terminating semi-colon (;).
 *
 * SECTION: Translators/Translator Declarations
 *
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

translator struct output_struct < struct input_struct *uvar >
{
	myi = ((struct input_struct *) uvar)->i;
	myc = ((struct input_struct *) uvar)->c;
}

translator struct output_struct < struct new_struct *newvar >
{
};


BEGIN
{
	printf("Test translator declaration without terminating semi-colon");
	exit(0);
}
