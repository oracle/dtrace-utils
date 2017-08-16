/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Use the translators in /usr/lib/dtrace/procfs.d
 *
 * SECTION: Translators/ Translator Declarations
 * SECTION: Translators/ Translate Operator
 * SECTION: Translators/Process Model Translators
 *
 */

#pragma D option quiet

struct task_struct *T;

BEGIN
{
	mypr_addr = xlate < psinfo_t > (T).pr_addr;
	printf("pr_addr: %d", mypr_addr);
	exit(0);
}

ERROR
{
	exit(1);
}
