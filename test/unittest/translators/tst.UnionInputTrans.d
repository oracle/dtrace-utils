/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Test the declaration of translators with a union to be the input type.
 *
 * SECTION: Translators/Translator Declarations
 *
 *
 */

#pragma D option quiet

union input_union {
	int i;
	char c;
} *ivar;

struct output_struct {
	int myi;
	char myc;
};

translator struct output_struct < union input_union *ivar >
{
	myi = ((union input_union *) ivar)->i;
	myc = ((union input_union *) ivar)->c;

};

BEGIN
{
	printf("Translator definition good\n");
	exit(0);
}
