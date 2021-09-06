/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test the normal behavior when discarding a speculative
 * default clause with quiet mode turned off.
 *
 * SECTION: Speculative Tracing/Discarding a Speculation;
 *	Actions and Subroutines/speculation()
 */

BEGIN
{
	self->spec = speculation();
}

BEGIN
/self->spec/
{
	speculate(self->spec);
}

BEGIN
/self->spec/
{
	discard(self->spec);
}

BEGIN
/self->spec/
{
	exit(0);
}
