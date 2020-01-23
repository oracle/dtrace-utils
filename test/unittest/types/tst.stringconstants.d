/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *	verify the use of string type
 *
 * SECTION: Types, Operators, and Expressions/Constants
 */

#pragma D option quiet

string string_1;

BEGIN
{
	string_1 = "abcd\n\n\nefg";
	string_2 = "abc\"\t\044\?\x4D";
	string_3 = "\?\\\'\"\0";

	printf("string_1 = %s\n", string_1);
	printf("string_2 = %s\n", string_2);
	printf("string_3 = %s\n", string_3);

	exit(0);
}
