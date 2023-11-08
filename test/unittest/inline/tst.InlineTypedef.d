/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Create inline names from aliases created using typedef.
 *
 * SECTION: Type and Constant Definitions/Inlines
 *
 * NOTES:
 *
 */

#pragma D option quiet


typedef char new_char;
inline new_char char_var = 'c';

typedef unsigned long * pointer;
inline pointer p = (pointer)&`max_pfn;

BEGIN
{
	printf("char_var: %c\npointer p: %d", char_var, *p);
	exit(0);
}
