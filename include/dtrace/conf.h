/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2009, 2019, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * Note: The contents of this file are private to the implementation of the
 * DTrace subsystem and are subject to change at any time without notice.
 */

#ifndef _DTRACE_CONF_H
#define _DTRACE_CONF_H

#include <dtrace/universal.h>
#include <dtrace/conf_defines.h>

typedef struct cpuinfo {
	processorid_t	cpu_id;
	psetid_t	cpu_pset;
	chipid_t	cpu_chip;
	lgrp_id_t	cpu_lgrp;
	void		*cpu_info;
} cpuinfo_t;

typedef struct dtrace_conf {
	uint32_t	num_possible_cpus;	/* number of possible CPUs */
	uint32_t	num_online_cpus;	/* number of online CPUs */
	processorid_t	max_cpuid;		/* highest CPU id */
	cpuinfo_t	*cpus;			/* CPU info structs */

	/* FIXME: Are these actually still necessary for our purposes? */
	uint32_t	dtc_difversion;	/* supported DIF version */
	uint32_t	dtc_difintregs;	/* # of DIF integer registers */
	uint32_t	dtc_diftupregs;	/* # of DIF tuple registers */
	uint32_t	dtc_ctfmodel;	/* CTF data model */
	uint32_t	dtc_maxbufs;	/* max # of buffers */
	uint32_t	dtc_pad[7];	/* reserved for future use */
} dtrace_conf_t;

#endif /* _DTRACE_CONF_H */
