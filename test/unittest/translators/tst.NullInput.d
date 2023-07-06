/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Translators can receive NULL as input.
 *
 * SECTION: Translators/Translator Declarations
 * SECTION: Translators/Translate Operator
 *
 */

#pragma D option quiet

struct task_struct *T;

BEGIN
{
	T = 0;
	mypr_addr = xlate < psinfo_t > (T).pr_addr;
	printf("pr_addr: %p", mypr_addr);
	exit(0);
}

ERROR
{
	exit(1);
}
