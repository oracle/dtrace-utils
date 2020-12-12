/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Test the declaration of a translator with a non-existent member
 *
 * SECTION: Translators/Translator Declarations
 *
 *
 */

#pragma D option quiet

struct input_struct {
	int i;
	char c;
} *ivar;

struct output_struct {
	int myi;
	char myc;
};

translator struct output_struct < struct input_struct *ivar >
{
	myi = ((struct input_struct *)ivar)->i;
	myc = ((struct input_struct *)ivar)->nonexistent;
};

BEGIN
{
	printf("Testing the assignment of a non-existent input member");
	exit(0);
}
