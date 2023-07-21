/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_RODATA_H
#define	_DT_RODATA_H

#include <sys/types.h>
#include <sys/dtrace_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dt_rodata	dt_rodata_t;

typedef ssize_t dt_rodata_copy_f(const char *, size_t, size_t, void *);

extern dt_rodata_t *dt_rodata_create(size_t);
extern void dt_rodata_destroy(dt_rodata_t *);
extern ssize_t dt_rodata_index(dt_rodata_t *, const char *, size_t);
extern ssize_t dt_rodata_insert(dt_rodata_t *, const char *, size_t);
extern size_t dt_rodata_size(const dt_rodata_t *);
extern size_t dt_rodata_max_item_size(const dt_rodata_t *);
extern ssize_t dt_rodata_copy(const char *, size_t, size_t, char *);
extern ssize_t dt_rodata_write(const dt_rodata_t *, dt_rodata_copy_f *,
			       void *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_RODATA_H */
