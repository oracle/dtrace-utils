/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Translate the input expression to output struct type
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
	myi = ((struct myinput_struct)ivar).i;
	myc = ((struct myinput_struct)ivar).c;

};

struct myinput_struct f;
BEGIN
{
	f.i = 10;
	f.c = 'c';

	realmyi = xlate < struct myoutput_struct > (f).myi;
	realmyc = xlate < struct myoutput_struct > (f).myc;
}

BEGIN
/(10 != f.i) || ('c' != f.c)/
{
	exit(1);
}

BEGIN
/(10 == f.i) && ('c' == f.c)/
{
	printf("realmyi: %d\n", realmyi);
	printf("realmyc: %c\n", realmyc);
	exit(0);
}

ERROR
{
	exit(1);
}
