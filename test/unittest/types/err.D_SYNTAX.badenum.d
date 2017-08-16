/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Test a bogus enum declaration using an invalid identifer.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 */


enum foo`bar
{

};
