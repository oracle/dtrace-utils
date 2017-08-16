/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * The output type in a translator definition can be a union.
 *
 * SECTION: Translators/Translator Declarations
 *
 *
 */

#pragma D option quiet

struct input_struct {
	int ii;
	char ic;
} *ivar;

union output_union {
	int oi;
	char oc;
};

translator union output_union < struct input_struct *ivar >
{
	oi = ((struct input_struct *) ivar)->ii;
	oc = ((struct input_struct *) ivar)->ic;

};

BEGIN
{
	printf("Test translator definition with union output");
	exit(0);
}
