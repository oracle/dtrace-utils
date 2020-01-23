/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION: Exit after commit should throw a D_DREC_COMM.
 *
 * SECTION: Speculative Tracing/Committing a Speculation
 *
 */
#pragma D option quiet

BEGIN
{
	self->i = 0;
	self->spec = speculation();
}

BEGIN
/self->spec/
{
	speculate(self->spec);
	self->i++;
	printf("self->i: %d\n", self->i);
}

BEGIN
/self->i/
{
	commit(self->spec);
	exit(0);
}

ERROR
{
	exit(0);
}
