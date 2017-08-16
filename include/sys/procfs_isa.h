/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _SYS_PROCFS_ISA_H
#define	_SYS_PROCFS_ISA_H

/*
 * Instruction Set Architecture specific component of <sys/procfs.h>
 * i386 version
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Possible values of pr_dmodel.
 * This isn't isa-specific, but it needs to be defined here for other reasons.
 */
#define	PR_MODEL_UNKNOWN 0
#define	PR_MODEL_ILP32	1	/* process data model is ILP32 */
#define	PR_MODEL_LP64	2	/* process data model is LP64 */


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_ISA_H */
