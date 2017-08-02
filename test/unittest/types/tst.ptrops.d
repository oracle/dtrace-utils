/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Verify relational operators with pointers
 *
 * SECTION: Types, Operators, and Expressions/Relational Operators;
 *	Types, Operators, and Expressions/Logical Operators;
 *	Types, Operators, and Expressions/Precedence
 *
 */

#pragma D option quiet


BEGIN
{
	ptr_1 = &`max_pfn;
	ptr_2 = (&`max_pfn) + 1;
	ptr_3 = (&`max_pfn) - 1 ;
}

tick-1
/ptr_1 >= ptr_2 || ptr_2 <= ptr_1 || ptr_1 == ptr_2/
{
	printf("Shouldn't end up here (1)\n");
	printf("ptr_1 = %x ptr_2 = %x ptr_3 = %x\n",
		(int) ptr_1, (int) ptr_2, (int) ptr_3);
	exit(1);
}

tick-1
/ptr_3 > ptr_1 || ptr_1 < ptr_3 || ptr_3 == ptr_1/
{
	printf("Shouldn't end up here (2)\n");
	printf("ptr_1 = %x ptr_2 = %x ptr_3 = %x\n",
		(int) ptr_1, (int) ptr_2, (int) ptr_3);
	exit(1);
}

tick-1
/ptr_3 > ptr_2 || ptr_1 < ptr_2 ^^ ptr_3 == ptr_2 && !(ptr_1 != ptr_2)/
{
	exit(0);
}
