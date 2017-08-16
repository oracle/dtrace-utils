/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 *	Test an illegal cast - make sure dtrace catches it.
 *
 * SECTION: Aggregations/Clearing aggregations
 *
 *
 */


BEGIN
{
	(struct task_struct)trace(`max_pfn);
	exit();
}
