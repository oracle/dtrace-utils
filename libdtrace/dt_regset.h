/*
 * Oracle Linux DTrace.
 * Copyright (c) 2003, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_REGSET_H
#define	_DT_REGSET_H

#include <sys/types.h>
#include <sys/dtrace_types.h>

#include <dt_as.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef void (*dt_cg_spill_f)(int reg);

typedef struct dt_regset {
	ulong_t		dr_size;	/* number of registers in set */
	dt_cg_spill_f	dr_spill_store;	/* register spill store function */
	dt_cg_spill_f	dr_spill_load;	/* register spill load function */
	ulong_t		*dr_active;	/* bitmap of active registers */
	ulong_t		*dr_spilled;	/* bitmap of spilled registers */
} dt_regset_t;

extern dt_regset_t *dt_regset_create(ulong_t, dt_cg_spill_f, dt_cg_spill_f);
extern void dt_regset_destroy(dt_regset_t *);
extern void dt_regset_reset(dt_regset_t *);
extern int dt_regset_alloc(dt_regset_t *);
extern int dt_regset_xalloc(dt_regset_t *, int);
extern void dt_regset_free(dt_regset_t *, int);
extern int dt_regset_xalloc_args(dt_regset_t *);
extern void dt_regset_free_args(dt_regset_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_REGSET_H */
