/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	verify the use of char constants
 *
 * SECTION: Types, Operators, and Expressions/Constants
 *
 */


#pragma D option quiet


BEGIN
{
	char_1 = 'a';
	char_2 = '\"';
	char_3 = '\"\ba';
	char_4 = '\?';
	char_5 = '\'';
	char_6 = '\\';
	char_7 = '\0103';
	char_8 = '\x4E';
	char_9 = '\c';		/* Note - this is not an escape sequence */
	char_10 = 'ab\"d';
	char_11 = 'a\bcdefgh';

	printf("decimal value = %d; character value = %c\n", char_1, char_1);
	printf("decimal value = %d; character value = %c\n", char_2, char_2);
	printf("decimal value = %d; character value = %c\n", char_3, char_3);
	printf("decimal value = %d; character value = %c\n", char_4, char_4);
	printf("decimal value = %d; character value = %c\n", char_5, char_5);
	printf("decimal value = %d; character value = %c\n", char_6, char_6);
	printf("decimal value = %d; character value = %c\n", char_7, char_7);
	printf("decimal value = %d; character value = %c\n", char_8, char_8);
	printf("decimal value = %d; character value = %c\n", char_9, char_9);
	printf("decimal value = %d; character value = %c\n", char_10, char_10);
	printf("decimal value = %d\n", char_11);

	exit(0);
}
