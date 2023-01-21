/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2021, Oracle and/or its affiliates. All rights reserved.
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
typedef void *(*htab_next_fn)(void *);

#define DEFINE_HE_STD_LINK_FUNCS(ID, TYPE, HE) \
	static TYPE *ID##_add(TYPE *head, TYPE *new) \
	{ \
		if (!head) \
			return new; \
	\
		new->HE.next = head; \
		head->HE.prev = new; \
	\
		return new; \
	} \
	\
	static TYPE *ID##_del(TYPE *head, TYPE *ent) \
	{ \
		TYPE	*prev = ent->HE.prev; \
		TYPE	*next = ent->HE.next; \
	\
		if (head == ent) { \
			if (!next) \
				return NULL; \
	\
			head = next; \
			head->HE.prev = NULL; \
			ent->HE.next = NULL; \
	\
			return head; \
		} \
	\
		if (!next) { \
			prev->HE.next = NULL; \
			ent->HE.prev = NULL; \
	\
			return head; \
		} \
	\
		prev->HE.next = next; \
		next->HE.prev = prev; \
		ent->HE.prev = ent->HE.next = NULL; \
	\
		return head; \
	} \
	  \
	static TYPE *ID##_next(TYPE *ent) \
	{ \
		return ent->HE.next; \
	}

#define DEFINE_HTAB_STD_OPS(ID) \
	static dt_htab_ops_t ID##_htab_ops = { \
		.hval = (htab_hval_fn)ID##_hval, \
		.cmp = (htab_cmp_fn)ID##_cmp, \
		.add = (htab_add_fn)ID##_add, \
		.del = (htab_del_fn)ID##_del, \
		.next = (htab_next_fn)ID##_next, \
	};


typedef struct dt_htab_ops {
	htab_hval_fn	hval;
	htab_cmp_fn	cmp;
	htab_add_fn	add;
	htab_del_fn	del;
	htab_next_fn	next;
} dt_htab_ops_t;

typedef struct dt_hentry {
	void		*next;
	void		*prev;
} dt_hentry_t;

typedef struct dt_htab		dt_htab_t;
typedef struct dt_htab_next	dt_htab_next_t;

extern dt_htab_t *dt_htab_create(struct dtrace_hdl *dtp, dt_htab_ops_t *ops);
extern void dt_htab_destroy(struct dtrace_hdl *dtp, dt_htab_t *htab);
extern int dt_htab_insert(dt_htab_t *htab, void *entry);
extern void *dt_htab_lookup(const dt_htab_t *htab, const void *entry);
typedef int dt_htab_ecmp_fn(const void *entry, void *arg);
extern void *dt_htab_find(const dt_htab_t *htab, const void *entry,
			  dt_htab_ecmp_fn *cmpf, void *arg);
extern size_t dt_htab_entries(const dt_htab_t *htab);
extern int dt_htab_delete(dt_htab_t *htab, void *entry);
extern void *dt_htab_next(const dt_htab_t *htab, dt_htab_next_t **it);
extern void dt_htab_next_destroy(dt_htab_next_t *i);
extern void dt_htab_stats(const char *name, const dt_htab_t *htab);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_HASH_H */
