/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2009, 2013, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * Note: The contents of this file are private to the implementation of the
 * DTrace subsystem and are subject to change at any time without notice.
 */

#ifndef _DTRACE_CONF_H
#define _DTRACE_CONF_H

#include <dtrace/universal.h>
#include <dtrace/conf_defines.h>

typedef struct dtrace_conf {
	uint_t		numcpus;	/* number of online CPUs */
	processorid_t	maxcpuid;	/* highest CPU id */
	processorid_t	*cpuids;	/* list of CPU ids */

	/* FIXME: Are these actually still necessary for our purposes? */
	uint_t		dtc_difversion;	/* supported DIF version */
	uint_t		dtc_difintregs;	/* # of DIF integer registers */
	uint_t		dtc_diftupregs;	/* # of DIF tuple registers */
	uint_t		dtc_ctfmodel;	/* CTF data model */
	uint_t		dtc_maxbufs;	/* max # of buffers */
	uint_t		dtc_pad[7];	/* reserved for future use */
} dtrace_conf_t;

#endif /* _DTRACE_CONF_H */
