/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 *      +=, -= must handle invalid variables.
 *
 * SECTION: Types, Operators, and Expressions/Assignment Operators
 *
 */


BEGIN
{
	p -= trace(0);
	exit(0);
}
