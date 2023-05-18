/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:  Check the constants used to access uregs[].
 *	       On x86, certain high indices are used to access not
 *	       pt_regs[] but curthread->thread.xxx.  So check them.
 *
 * SECTION: User Process Tracing/uregs Array
 */

#pragma D option quiet

BEGIN
{
	printf("%x %x\n", uregs[R_DS    ], curthread->thread.ds);
	printf("%x %x\n", uregs[R_ES    ], curthread->thread.ds);
	printf("%x %x\n", uregs[R_FS    ], curthread->thread.fsbase);
	printf("%x %x\n", uregs[R_GS    ], curthread->thread.gsbase);
	printf("%x %x\n", uregs[R_TRAPNO], curthread->thread.trap_nr);

        exit(0);
}

ERROR
{
	exit(1);
}
