/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test that a load-before-store pattern for a local variable forces
 *	      the local variable to be initialized prior to loading.
 *
 * FAILURE:   If the initialization is not done, the BPF verifier will reject
 *	      the program due to reading from an uninitialized stack location.
 *
 * SECTION: Variables/Clause-Local Variables
 */

BEGIN
{
	++this->x;
	exit(0);
}
