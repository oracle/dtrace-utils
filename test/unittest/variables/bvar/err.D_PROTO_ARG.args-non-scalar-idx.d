/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The index of args[] must be a scalar.
 *
 * SECTION: Variables/Built-in Variables/args
 */

BEGIN
{
	trace(args["a"]);
	exit(0);
}

ERROR
{
	exit(1);
}
