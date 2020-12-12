/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Assign the different members of the output of the translator using
 * different translator declarations.
 *
 * SECTION: Translators/Translator Declarations
 *
 *
 */

#pragma D option quiet

struct input_struct1 {
	int ii1;
	char ic1;
} *ivar1;

struct input_struct2 {
	int ii2;
	char ic2;
} *ivar2;

struct output_struct {
	int oi;
	char oc;
};


translator struct output_struct < struct input_struct1 *ivar1 >
{
	oi = ((struct input_struct1 *)ivar1)->ii1;
};

translator struct output_struct < struct input_struct2 *ivar2 >
{
	oc = ((struct input_struct2 *)ivar2)->ic2;
};

BEGIN
{
	printf("Translate members of output with different input structs");
	exit(0);
}
