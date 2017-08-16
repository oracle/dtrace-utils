/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_DOF_H
#define	_DT_DOF_H

#include <dtrace.h>

#ifdef	__cplusplus
extern "C" {
#endif

#include <dt_buf.h>

typedef struct dt_dof {
	dtrace_hdl_t *ddo_hdl;		/* libdtrace handle */
	dtrace_prog_t *ddo_pgp;		/* current program */
	uint_t ddo_nsecs;		/* number of sections */
	dof_secidx_t ddo_strsec; 	/* global strings section index */
	dof_secidx_t *ddo_xlimport;	/* imported xlator section indices */
	dof_secidx_t *ddo_xlexport;	/* exported xlator section indices */
	dt_buf_t ddo_secs;		/* section headers */
	dt_buf_t ddo_strs;		/* global strings */
	dt_buf_t ddo_ldata;		/* loadable section data */
	dt_buf_t ddo_udata;		/* unloadable section data */
	dt_buf_t ddo_probes;		/* probe section data */
	dt_buf_t ddo_args;		/* probe arguments section data */
	dt_buf_t ddo_offs;		/* probe offsets section data */
	dt_buf_t ddo_enoffs;		/* is-enabled offsets section data */
	dt_buf_t ddo_rels;		/* probe relocation section data */
	dt_buf_t ddo_xlms;		/* xlate members section data */
} dt_dof_t;

extern void dt_dof_init(dtrace_hdl_t *);
extern void dt_dof_fini(dtrace_hdl_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_DOF_H */
