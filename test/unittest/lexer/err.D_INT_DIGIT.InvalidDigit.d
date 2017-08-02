/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: If an integer representation contains an invalid digit
 * a D_INT_DIGIT is thrown.
 *
 * SECTION: Errtags/D_INT_DIGIT
 */

BEGIN
{
	0128;
}
