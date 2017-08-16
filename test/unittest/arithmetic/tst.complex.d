/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Complex expressions.
 *	Call complex expressions and make sure test succeeds.
 *	Match expected output in tst.complex.d.out
 *
 * SECTION: Types, Operators, and Expressions/Arithmetic Operators
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
	i = i++ + ++i;
	printf("The value of i is %d\n", i);
	i = i-- - --i;
	printf("The value of i is %d\n", i);
	i = i-- + ++i;
	printf("The value of i is %d\n", i);
	i += i++ + -- i + ++i - ++i * i ;
	printf("The value of i is %d\n", i);
	i -= i++ * 3;
	printf("The value of i is %d\n", i);
	i = i++/i--+i++-++i-++i;
	printf("The value of i is %d\n", i);
	exit (0);
}
