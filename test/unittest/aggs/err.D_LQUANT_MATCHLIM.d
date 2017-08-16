/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

BEGIN
{
	@ = lquantize(0, 10, 20, 1);
	@ = lquantize(0, 10, 2000, 1);
}
