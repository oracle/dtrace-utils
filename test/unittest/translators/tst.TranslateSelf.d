/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * Test the translation of a struct to itself.
 *
 * SECTION: Translators/ Translator Declarations.
 * SECTION: Translators/ Translate operator.
 *
 *
 */

#pragma D option quiet

struct output_struct {
	int myi;
	char myc;
};

translator struct output_struct < struct output_struct uvar >
{
	myi = ((struct output_struct)uvar).myi;
	myc = ((struct output_struct)uvar).myc;
};

struct output_struct out;
struct output_struct outer;

BEGIN
{
	out.myi = 1234;
	out.myc = 'a';

	printf("Test translation of a struct to itself\n");
	outer = xlate < struct output_struct > (out);

	printf("outer.myi: %d\t outer.myc: %c\n", outer.myi, outer.myc);
}

BEGIN
/(1234 != outer.myi) || ('a' != outer.myc)/
{
	exit(1);
}

BEGIN
/(1234 == outer.myi) && ('a' == outer.myc)/
{
	exit(0);
}
