/*
 * Oracle Linux DTrace.
 * Copyright © 2006, 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Redeclaration of a translator is forbidden.
 *
 * SECTION: Errtag/D_CG_DYN
 */

#pragma D option quiet

translator lwpsinfo_t < struct task_struct *T >
{
	pr_flag = (T->state & __TASK_STOPPED) ? PR_STOPPED : 0;
	pr_lwpid = T->pid;
};

BEGIN
{
	xlate < lwpsinfo_t * > (curthread);
	exit(0);
}
