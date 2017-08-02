/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PROGRAM_H
#define	_DT_PROGRAM_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <dtrace.h>
#include <dt_list.h>

typedef struct dt_stmt {
	dt_list_t ds_list;	/* list forward/back pointers */
	dtrace_stmtdesc_t *ds_desc; /* pointer to statement description */
} dt_stmt_t;

struct dtrace_prog {
	dt_list_t dp_list;	/* list forward/back pointers */
	dt_list_t dp_stmts;	/* linked list of dt_stmt_t's */
	ulong_t **dp_xrefs;	/* array of translator reference bitmaps */
	uint_t dp_xrefslen;	/* length of dp_xrefs array */
	uint8_t dp_dofversion;	/* DOF version this program requires */
};

extern dtrace_prog_t *dt_program_create(dtrace_hdl_t *);
extern void dt_program_destroy(dtrace_hdl_t *, dtrace_prog_t *);

extern dtrace_ecbdesc_t *dt_ecbdesc_create(dtrace_hdl_t *,
    const dtrace_probedesc_t *);
extern void dt_ecbdesc_release(dtrace_hdl_t *, dtrace_ecbdesc_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PROGRAM_H */
