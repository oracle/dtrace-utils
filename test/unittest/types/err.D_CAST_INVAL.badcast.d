/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test an illegal cast - make sure dtrace catches it.
 */

BEGIN
{
	(struct task_struct)trace(`max_pfn);
	exit(0);
}
