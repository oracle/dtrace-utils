/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * Test the declaration of a translator the input type in the translator
 * declaration being different from the values being assigned inside.
 *
 * SECTION: Translators/Translator Declarations
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

translator struct output_struct < int idontcare >
{
	myi = ((struct input_struct *) uvar)->i;
	myi = ((struct input_struct *) uvar)->c;
};

BEGIN
{
	printf("Input type to the translator decl is different from members");
	exit(0);
}
