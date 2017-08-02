/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Redeclaring the same translation twice throws a D_XLATE_REDECL error.
 *
 * SECTION: Translators/Translator Declarations
 *
 */

#pragma D option quiet

struct input_struct {
	int ii1;
	char ic1;
} *ivar1;

struct output_struct {
	int oi;
	char oc;
};


translator struct output_struct < struct input_struct *ivar1 >
{
	oi = ((struct input_struct *) ivar1)->ii1;
	oc = ((struct input_struct *) ivar1)->ic1;
};

translator struct output_struct < struct input_struct *ivar1 >
{
	oi = ((struct input_struct *) ivar1)->ii1;
	oc = ((struct input_struct *) ivar1)->ic1;
};


BEGIN
{
	printf("Redeclaration of the same translation");
	exit(0);
}
