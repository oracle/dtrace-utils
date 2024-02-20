/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PID_H
#define	_DT_PID_H

#include <libproc.h>
#include <dtrace/pid.h>
#include <dt_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int dt_pid_create_probes(dtrace_probedesc_t *, dtrace_hdl_t *,
				dt_pcb_t *);
extern int dt_pid_create_probes_module(dtrace_hdl_t *, dt_proc_t *);
extern pid_t dt_pid_get_pid(const dtrace_probedesc_t *, dtrace_hdl_t *, dt_pcb_t *,
			    dt_proc_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PID_H */
