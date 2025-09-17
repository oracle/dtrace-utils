/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./002intro-global-variables.d
 *
 *  DESCRIPTION
 *    Rules for variable names are similar to those in C.  By
 *    default, they are "global" -- that is, seen by every thread
 *    and every probe.  So be careful that no thread overwrites any
 *    other thread's write.  Variables may be typed implicitly.
 */

int explicitly_typed_variable;

dtrace:::BEGIN
{
	explicitly_typed_variable = 1234;
	implicitly_typed_variable = 5678;

	printf("%d %d\n", explicitly_typed_variable,
			  implicitly_typed_variable);

	my_char = 'a';
	my_string = "hello world";
	my_pointer = (int *) 0x123456;

	printf("%c '%s' %p\n", my_char, my_string, my_pointer);

	exit(0);
}
