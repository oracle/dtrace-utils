/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * A translator declaration may omit expressions for one or more members
 * of the output type
 *
 * SECTION: Translators/Translator Declarations;
 * 	Translators/Translate Operator
 */

#pragma D option quiet

struct input_struct {
	int ii;
	char ic;
};

struct output_struct {
	int oi;
	char oc;
};


translator struct output_struct < struct input_struct *ivar >
{
	oi = ((struct input_struct *) ivar)->ii;
};

BEGIN
{
	printf("Translating only a part of the input struct");
	exit(0);
}
