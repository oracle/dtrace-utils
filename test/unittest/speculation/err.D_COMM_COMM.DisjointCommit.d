/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * A clause cannot contain multiple commit() calls to disjoint buffers.
 *
 * SECTION: Speculative Tracing/Committing a Speculation;
 *	Options and Tunables/cleanrate
 */
#pragma D option quiet
#pragma D option cleanrate=3000hz

BEGIN
{
	self->i = 0;
	self->j = 0;
	self->commit = 0;
	var1 = 0;
	var2 = 0;
}

BEGIN
{
	var1 = speculation();
	printf("Speculation ID: %d\n", var1);
}

BEGIN
{
	var2 = speculation();
	printf("Speculation ID: %d\n", var2);
}

BEGIN
/var1/
{
	speculate(var1);
	printf("Speculating on id: %d\n", var1);
	self->i++;
}

BEGIN
/var2/
{
	speculate(var2);
	printf("Speculating on id: %d", var2);
	self->j++;

}

BEGIN
/(self->i) && (self->j)/
{
	commit(var1);
	commit(var2);
	self->commit++;
}

BEGIN
/self->commit/
{
	printf("Succesfully commited both buffers");
	exit(0);
}

BEGIN
/!self->commit/
{
	printf("Couldnt commit both buffers");
	exit(1);
}

ERROR
{
	exit(1);
}
