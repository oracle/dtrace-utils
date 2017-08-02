/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * When the output type of the translator declaration is not a struct or
 * union a D_XLATE_SOU is thrown.
 *
 * SECTION: Translators/Translator Declarations
 */

#pragma D option quiet

struct input_struct {
	int ii;
	char ic;
};

enum colors
{
	RED,
	BLUE
};

translator enum colors < struct input_struct *ivar >
{
};

BEGIN
{
	printf("Output type of translation is an enum");
	exit(0);
}
