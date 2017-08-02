/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * When xlate is used on a variable for which no translation exists a
 * D_XLATE_NONE is thrown
 *
 * SECTION: Translators/Translate Operator
 *
 *
 */

#pragma D option quiet

struct myinput_struct {
	int i;
	char c;
};

struct myoutput_struct {
	int myi;
	char myc;
};

translator struct myoutput_struct < struct myinput_struct *ivar >
{
	myi = ((struct myinput_struct *) ivar)->i;
	myc = ((struct myinput_struct *) ivar)->c;

};

struct myinput_struct f;
BEGIN
{
	f.i = 10;
	f.c = 'c';

	xlate < struct myoutput_struct >(f)->myi;
	printf("Translate operator used without correct translator decl\n");
	exit(0);
}

ERROR
{
	exit(0);
}
