/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_STRTAB_H
#define	_DT_STRTAB_H

#include <sys/types.h>
#include <sys/dtrace_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dt_strtab	dt_strtab_t;

typedef ssize_t dt_strtab_write_f(const char *, size_t, size_t, void *);

extern dt_strtab_t *dt_strtab_create(size_t);
extern void dt_strtab_destroy(dt_strtab_t *);
extern ssize_t dt_strtab_index(dt_strtab_t *, const char *);
extern ssize_t dt_strtab_insert(dt_strtab_t *, const char *);
extern size_t dt_strtab_size(const dt_strtab_t *);
extern ssize_t dt_strtab_copystr(const char *, size_t, size_t, char *);
extern ssize_t dt_strtab_write(const dt_strtab_t *,
    dt_strtab_write_f *, void *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_STRTAB_H */
