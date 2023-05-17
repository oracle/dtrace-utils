/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2009, 2023, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * Note: The contents of this file are private to the implementation of the
 * DTrace subsystem and are subject to change at any time without notice.
 */

#ifndef _DTRACE_PID_H
#define _DTRACE_PID_H

#include <sys/types.h>
#include <dirent.h>
#include <dtrace/universal.h>

typedef enum pid_probetype {
	DTPPT_NONE = 0,
	DTPPT_ENTRY,
	DTPPT_RETURN,
	DTPPT_OFFSETS,
	DTPPT_POST_OFFSETS,
	DTPPT_IS_ENABLED
} pid_probetype_t;

typedef struct pid_probespec {
	pid_probetype_t pps_type;		/* probe type */
	char *pps_prv;				/* provider (without pid) */
	char *pps_mod;				/* probe module (object) */
	char *pps_fun;				/* probe function */
	char *pps_prb;				/* probe name (if provided) */
	dev_t pps_dev;				/* object device node */
	ino_t pps_inum;				/* object inode */
	char *pps_fn;				/* object full filename */
	uint64_t pps_off;			/* probe offset (in object) */

	/*
	 * Fields below this point do not apply to probes of type
	 * DTPPT_UNDERLYING.
	 */
	pid_t pps_pid;				/* task PID */
	uint64_t pps_vaddr;			/* object base address */
	uint64_t pps_size;			/* function size (in bytes) */
	char pps_gstr[1];			/* glob pattern string */
} pid_probespec_t;

#endif /* _DTRACE_PID_H */
