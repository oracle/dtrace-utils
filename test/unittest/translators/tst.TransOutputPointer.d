/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Translate an input expression to a pointer to a struct
 *
 * SECTION: Translators/ Translator Declarations
 * SECTION: Translators/ Translate Operator
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

translator struct myoutput_struct < struct myinput_struct ivar >
{
	myi = ((struct myinput_struct ) ivar).i;
	myc = ((struct myinput_struct ) ivar).c;

};

struct myinput_struct f;

BEGIN
{
	f.i = 1203;
	f.c = 'v';

	realmyi = xlate < struct myoutput_struct *> (f)->myi;
	realmyc = xlate < struct myoutput_struct *> (f)->myc;
}

BEGIN
/(1203 != realmyi) || ('v' != realmyc)/
{
	printf("Failure");
	exit(1);
}

BEGIN
/(1203 == realmyi) && ('v' == realmyc)/
{
	printf("Success");
	exit(0);
}
