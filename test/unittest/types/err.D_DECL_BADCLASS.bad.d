/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Invalid class names should be handled as an error.
 *
 * SECTION: Types, Operators, and Expressions/Data Types and Sizes
 *
 */

register x;


BEGIN
{
	exit(0);
}
