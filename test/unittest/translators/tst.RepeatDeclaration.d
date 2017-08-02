/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Reassign the members of the output struct using another translator
 * input struct.
 *
 * SECTION: Translators/Translator Declarations
 */

#pragma D option quiet

struct input_struct1 {
	int ii1;
	char ic1;
} *ivar1;

struct input_struct2 {
	long ii2;
	long ic2;
} *ivar2;

struct output_struct {
	int oi;
	char oc;
};


translator struct output_struct < struct input_struct1 *ivar1 >
{
	oi = ((struct input_struct1 *) ivar1)->ii1;
	oc = ((struct input_struct1 *) ivar1)->ic1;
};

translator struct output_struct < struct input_struct2 *ivar2 >
{
	oi = ((struct input_struct2 *) ivar2)->ii2;
	oc = ((struct input_struct2 *) ivar2)->ic2;
};

BEGIN
{
	printf("Reassignment of a struct's members with different input");
	exit(0);
}
