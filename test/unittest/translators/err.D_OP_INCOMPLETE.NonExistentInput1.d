/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Test the declaration of a translator with a non-existent input type.
 *
 * SECTION: Translators/Translator Declarations
 *
 */

#pragma D option quiet

struct output_struct {
	int myi;
	char myc;
};

translator struct output_struct < struct input_struct *ivar >
{
	myi = ((struct input_struct *) ivar)->nonexistentI;
	myc = ((struct input_struct *) ivar)->nonexistentC;
};

BEGIN
{
	printf("Test the translation of a non existing input type");
	exit(0);
}
