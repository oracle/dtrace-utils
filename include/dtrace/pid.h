/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2009, 2024, Oracle and/or its affiliates. All rights reserved.
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
	DTPPT_USDT,
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
	int pps_nargc;				/* number of native args */
	int pps_xargc;				/* number of xlated and mapped args */
	char *pps_nargv;			/* array of native args */
	size_t pps_nargvlen;			/* (high estimate of) length of array */
	char *pps_xargv;			/* array of xlated args */
	size_t pps_xargvlen;			/* (high estimate of) length of array */
	int8_t *pps_argmap;			/* mapped arg indexes */

	/*
	 * Fields below this point do not apply to underlying probes.
	 */
	pid_t pps_pid;				/* task PID */
	uint64_t pps_nameoff;			/* offset to use for name */
} pid_probespec_t;

#endif /* _DTRACE_PID_H */
