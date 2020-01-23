/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * The output type is an alias.
 *
 * SECTION: Translators/ Translator Declarations.
 *
 *
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

typedef struct output_struct output_t;


translator output_t < struct input_struct *ivar >
{
	oi = ((struct input_struct *) ivar)->ii;
	oc = ((struct input_struct *) ivar)->ic;
};

BEGIN
{
	printf("Output type is an alias");
	exit(0);
}
