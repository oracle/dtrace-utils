/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Test the normal declaration of translators.
 *
 * SECTION: Translators/Translator Declarations
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

struct myinput_struct *f;

BEGIN
{
	printf("Good translator defn");
	exit(0);
}
