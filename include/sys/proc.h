/*
 * Oracle Linux DTrace.
 * Copyright © 2011, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _FIX_SYS_PROC_H
#define _FIX_SYS_PROC_H

/* stat codes */
 
#define	SSLEEP	1		/* awaiting an event */
#define	SRUN	2		/* runnable */
#define	SZOMB	3		/* process terminated but not waited for */
#define	SSTOP	4		/* process stopped by debugger */
#define	SIDL	5		/* intermediate state in process creation */
#define	SONPROC	6		/* process is being run on a processor */
#define	SWAIT	7		/* process is waiting to become runnable */

#endif
