/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_HASH_H
#define	_DT_HASH_H

#ifdef	__cplusplus
extern "C" {
#endif

struct dtrace_hdl;

typedef uint32_t (*htab_hval_fn)(const void *);
typedef int (*htab_cmp_fn)(const void *, const void *);
typedef void *(*htab_add_fn)(void *, void *);
typedef void *(*htab_del_fn)(void *, void *);

typedef struct dt_htab_ops {
	htab_hval_fn	hval;
	htab_cmp_fn	cmp;
	htab_add_fn	add;
	htab_del_fn	del;
} dt_htab_ops_t;

typedef struct dt_hentry {
	void		*next;
	void		*prev;
} dt_hentry_t;

typedef struct dt_htab	dt_htab_t;

extern dt_htab_t *dt_htab_create(struct dtrace_hdl *dtp, dt_htab_ops_t *ops);
extern void dt_htab_destroy(struct dtrace_hdl *dtp, dt_htab_t *htab);
extern int dt_htab_insert(dt_htab_t *htab, void *entry);
extern void *dt_htab_lookup(const dt_htab_t *htab, const void *entry);
extern int dt_htab_delete(dt_htab_t *htab, void *entry);
extern void dt_htab_stats(const char *name, const dt_htab_t *htab);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_HASH_H */
