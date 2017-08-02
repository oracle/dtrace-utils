/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Test circular declaration of translations
 *
 * SECTION: Translators/ Translator Declarations
 * SECTION: Translators/ Translate Operator
 */

#pragma D option quiet

struct input_struct {
	int i;
	char c;
};

struct output_struct {
	int myi;
	char myc;
};

translator struct output_struct < struct input_struct ivar >
{
	myi = ((struct input_struct ) ivar).i;
	myc = ((struct input_struct ) ivar).c;
};

translator struct input_struct < struct output_struct uvar >
{
	i = ((struct output_struct ) uvar).myi;
	c = ((struct output_struct ) uvar).myc;
};

struct input_struct f1;
struct output_struct f2;

BEGIN
{
	f1.i = 10;
	f1.c = 'c';

	f2.myi = 100;
	f2.myc = 'd';

	printf("Testing circular translations\n");
	forwardi = xlate < struct output_struct > (f1).myi;
	forwardc = xlate < struct output_struct > (f1).myc;
	backwardi = xlate < struct input_struct > (f2).i;
	backwardc = xlate < struct input_struct > (f2).c;

	printf("forwardi: %d\tforwardc: %c\n", forwardi, forwardc);
	printf("backwardi: %d\tbackwardc: %c", backwardi, backwardc);
	exit(0);
}

BEGIN
/(10 == forwardi) && ('c' == forwardc) && (100 == backwardi) &&
    ('d' == backwardc)/
{
	exit(0);
}

BEGIN
/(10 != forwardi) || ('c' != forwardc) || (100 != backwardi) ||
    ('d' != backwardc)/
{
	exit(1);
}

ERROR
{
	exit(1);
}
