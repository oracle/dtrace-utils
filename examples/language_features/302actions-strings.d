/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

#pragma D option quiet

/*
 *  SYNOPSIS
 *    sudo ./302actions-strings.d
 *
 *  DESCRIPTION
 *    There are functions to manipulate strings.
 */

BEGIN
{
	/* find the length of a string */
	printf("length of 'abcde' is %d\n", strlen("abcde"));

	/* various other functions also resemble libc */
	printf("text starts with first 'd': %s\n", strchr("abcdefedbca", 'd'));
	printf("text starts with last 'd': %s\n", strrchr("abcdefedbca", 'd'));
	printf("text starts with 'def': %s\n", strstr("abcdefedbca", "def"));

	/* break a string up into tokens */
	printf("token #1: %s\n", strtok("hello world", " "));
	printf("token #2: %s\n", strtok(NULL, " "));

	/* join (concatenate) strings */
	printf("join 'abc' and 'def': %s\n", strjoin("abc", "def"));

	/* dump part of a string */
	printf("start at char #4: %s\n", substr("abcdefghij", 4));

	/* dump part of a string, only 2 chars */
	printf("start at char #4: %s\n", substr("abcdefghij", 4, 2));

	/* find index of a substring */
	printf("found at char #%d\n", index("abcdefghij", "def"));
	printf("found at char #%d\n", rindex("abcdefghij", "def"));
	printf("found at char #%d\n", index("abcdefghij", "XXXX"));

	/* convert an integer to a string */
	printf("123000 + 456 = %s\n", lltostr(123000 + 456));

	exit(0);
}
