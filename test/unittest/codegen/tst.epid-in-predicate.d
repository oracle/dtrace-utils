/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'epid' variable can be accessed in a predicate and is not 0.
 *
 * SECTION: Variables/Built-in Variables/epid
 */

#pragma D option quiet

BEGIN
/(this->a = epid) || 1/
{
	trace(this->a);
	exit(this->a != 0 && epid == this->a ? 0 : 1);
}
