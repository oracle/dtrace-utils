/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * The input type of the tranlator declaration is an alias.
 *
 * SECTION: Translators/ Translator Declarations.
 *
 */

#pragma D option quiet

typedef struct input_struct {
	int ii;
	char ic;
} input_t;

struct output_struct {
	int oi;
	char oc;
};


translator struct output_struct < input_t *ivar >
{
	oi = ((input_t *)ivar)->ii;
	oc = ((input_t *)ivar)->ic;
};

BEGIN
{
	printf("Input type is an alias");
	exit(0);
}
