/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

int me;			/* an integer global variable */
this int foo;		/* an integer clause-local variable */

tick-1sec
{
	/*
	 * Set foo to be 10 if and only if this is the first clause executed.
	 */
	this->foo = (me % 3 == 0) ? 10 : this->foo;
	printf("Clause 1 is number %d; foo is %d", me++ % 3, this->foo++);
}

tick-1sec
{
	/*
	 * Set foo to be 20 if and only if this is the first clause executed. 
	 */
	this->foo = (me % 3 == 0) ? 20 : this->foo;
	printf("Clause 2 is number %d; foo is %d", me++ % 3, this->foo++);
}

tick-1sec
{
	/*
	 * Set foo to be 30 if and only if this is the first clause executed.
	 */
	this->foo = (me % 3 == 0) ? 30 : this->foo;
	printf("Clause 3 is number %d; foo is %d", me++ % 3, this->foo++);
}

tick-1sec
/me >= 12/
{
	printf("Completed %d iterations.", me);
	exit(0);
}
