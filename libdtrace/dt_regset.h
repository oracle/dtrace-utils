/*
 * Oracle Linux DTrace.
 * Copyright © 2003, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_REGSET_H
#define	_DT_REGSET_H

#include <sys/types.h>
#include <sys/dtrace_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dt_regset {
	ulong_t dr_size;		/* number of registers in set */
	ulong_t *dr_bitmap;		/* bitmap of active registers */
} dt_regset_t;

extern dt_regset_t *dt_regset_create(ulong_t);
extern void dt_regset_destroy(dt_regset_t *);
extern void dt_regset_reset(dt_regset_t *);
extern int dt_regset_alloc(dt_regset_t *);
extern void dt_regset_free(dt_regset_t *, int);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_REGSET_H */
