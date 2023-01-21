/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * This file provides a generic hashtable implementation.
 *
 * Code that uses this must provide a dt_htab_ops_t struct with helper
 * functions to be used for manipulating the hashtable.  The following
 * functions must be provided:
 *	hval(void *e)		- calculate a hash value for the given entry
 *	cmp(void *e1, void *e2)	- compare two entries
 *	add(void *h, void *e)	- add an entry to a list (h is head)
 *	del(void *h, void *e)	- delete an entry from a list (h is head)
 *	next(void *e)		- return the next entry after e
 *
 * Entries are hashed into a hashtable slot based on the return value of
 * hval(e).  Each hashtable slot holds a list of buckets, with each bucket
 * storing entries that are equal under the cmp(e1, e2) function.  Entries are
 * added to the list of entries in a bucket using the add(h, e) function, and
 * they are deleted using a call to the del(h, e) function.
 */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "dt_impl.h"

typedef struct dt_hbucket	dt_hbucket_t;
struct dt_hbucket {
	uint32_t	hval;
	dt_hbucket_t	*next;
	void		*head;
	int		nentries;
};

struct dt_htab {
	dt_hbucket_t	**tab;
	int		size;
	int		mask;
	int		nbuckets;
	size_t		nentries;
	dt_htab_ops_t	*ops;
};

/*
 *  The dt_htab_next iteration state is somewhat unusual. It points not to the
 *  element most recently returned by the iterator, but to the element which
 *  will be returned on the *next* iteration cycle (so it spends the last
 *  iteration cycle "off-the-end", all-NULL).  This allows the user to
 *  delete the current element returned by dt_htab_next().
 */
struct dt_htab_next {
	const dt_htab_t	*htab;
	int		idx;
	dt_hbucket_t	*bucket;
	void		*ent;
	int		exhausted;
};

/*
 * Create a new (empty) hashtable.
 */
dt_htab_t *dt_htab_create(dtrace_hdl_t *dtp, dt_htab_ops_t *ops)
{
	dt_htab_t	*htab = dt_alloc(dtp, sizeof(dt_htab_t));

	if (!htab)
		return NULL;

	htab->size = 1;
	htab->mask = htab->size - 1;
	htab->nbuckets = 0;
	htab->nentries = 0;
	htab->ops = ops;

	htab->tab = dt_calloc(dtp, htab->size, sizeof(dt_hbucket_t *));
	if (!htab->tab) {
		dt_free(dtp, htab);
		return NULL;
	}

	return htab;
}

/*
 * Destroy a hashtable, deleting all its entries first.
 */
void dt_htab_destroy(dtrace_hdl_t *dtp, dt_htab_t *htab)
{
	size_t		i;

	if (!htab)
		return;

	for (i = 0; i < htab->size; i++) {
		dt_hbucket_t	*bucket = htab->tab[i];

		while (bucket) {
			dt_hbucket_t *obucket = bucket;

			while (bucket->head)
				bucket->head = htab->ops->del(bucket->head,
				    bucket->head);
			bucket = bucket->next;
			free(obucket);
		};
	}

	dt_free(dtp, htab->tab);
	dt_free(dtp, htab);
}

/*
 * Resize the hashtable by doubling the number of slots.
 */
static int resize(dt_htab_t *htab)
{
	int		i;
	int		osize = htab->size;
	int		nsize = osize << 1;
	int		nmask = nsize - 1;
	dt_hbucket_t	**ntab;

	ntab = calloc(nsize, sizeof(dt_hbucket_t *));
	if (!ntab)
		return -ENOMEM;

	for (i = 0; i < osize; i++) {
		dt_hbucket_t	*bucket, *next;

		for (bucket = htab->tab[i]; bucket; bucket = next) {
			int	idx	= bucket->hval & nmask;

			next = bucket->next;
			bucket->next = ntab[idx];
			ntab[idx] = bucket;
		}
	}

	free(htab->tab);
	htab->tab = ntab;
	htab->size = nsize;
	htab->mask = nmask;

	return 0;
}

/*
 * Add an entry to the hashtable.  Resize if necessary, and allocate a new
 * bucket if necessary.
 */
int dt_htab_insert(dt_htab_t *htab, void *entry)
{
	uint32_t	hval = htab->ops->hval(entry);
	int		idx;
	dt_hbucket_t	*bucket;

retry:
	idx = hval & htab->mask;
	for (bucket = htab->tab[idx]; bucket; bucket = bucket->next) {
		if (htab->ops->cmp(bucket->head, entry) == 0)
			goto add;
	}

	if ((htab->nbuckets >> 1) > htab->size) {
		int	err;

		err = resize(htab);
		if (err)
			return err;

		goto retry;
	}

	bucket = malloc(sizeof(dt_hbucket_t));
	if (!bucket)
		return -ENOMEM;

	bucket->hval = hval;
	bucket->next = htab->tab[idx];
	bucket->head = NULL;
	bucket->nentries = 0;
	htab->tab[idx] = bucket;
	htab->nbuckets++;

add:
	bucket->head = htab->ops->add(bucket->head, entry);
	bucket->nentries++;
	htab->nentries++;

	return 0;
}

/*
 * Find an entry in the hashtable.
 */
void *dt_htab_lookup(const dt_htab_t *htab, const void *entry)
{
	uint32_t	hval = htab->ops->hval(entry);
	int		idx = hval & htab->mask;
	dt_hbucket_t	*bucket;

	for (bucket = htab->tab[idx]; bucket; bucket = bucket->next) {
		if (htab->ops->cmp(bucket->head, entry) == 0)
			return bucket->head;
	}

	return NULL;
}

/*
 * Find an entry in the hashtable, using the provided callback function as a
 * secondary comparison function to differentiate between entries in a bucket.
 */
void *dt_htab_find(const dt_htab_t *htab, const void *entry,
		   dt_htab_ecmp_fn *cmpf, void *arg)
{
	void	*ent = dt_htab_lookup(htab, entry);

	while (ent != NULL && !cmpf(ent, arg))
		ent = htab->ops->next(ent);

	return ent;
}

/*
 * Remove an entry from the hashtable.  If we are deleting the last entry in a
 * bucket, get rid of the bucket.
 */
int dt_htab_delete(dt_htab_t *htab, void *entry)
{
	uint32_t	hval = htab->ops->hval(entry);
	int		idx = hval & htab->mask;
	dt_hbucket_t	*bucket;
	void		*head;

	/*
	 * Find the right bucket in cases of hash collision.
	 */

	for (bucket = htab->tab[idx]; bucket; bucket = bucket->next) {
		if (htab->ops->cmp(bucket->head, entry) == 0)
			break;
	}

	if (bucket == NULL)
		return -ENOENT;

	/*
	 * Delete the specified entry, now known to be in this bucket.
	 */
	head = htab->ops->del(bucket->head, entry);
	bucket->nentries--;
	htab->nentries--;

	/*
	 * Deal with a now-empty bucket.
	 */
	if (!head) {
		dt_hbucket_t	*b = htab->tab[idx];

		if (bucket == b)
			htab->tab[idx] = bucket->next;
		else {
			while (b->next != bucket)
				b = b->next;

			b->next = bucket->next;
		}

		htab->nbuckets--;
		free(bucket);
	} else
		bucket->head = head;

	return 0;
}

/*
 * Iterate over a hashtable, returning each element in turn (as a pointer to
 * void).  NULL is returned at end-of-iteration (and OOM).
 */
void *
dt_htab_next(const dt_htab_t *htab, dt_htab_next_t **it)
{
	dt_htab_next_t *i = *it;
	void *ret;

	/*
	 * Start of iteration.
	 */
	if (!i) {
		if ((i = calloc(1, sizeof(dt_htab_next_t))) == NULL)
			return NULL;
		i->htab = htab;
		*it = i;

		/*
		 * Tick through one iteration.  Ignore the return value,
		 * since it is meaningless at this stage.
		 */
		(void) dt_htab_next(htab, it);
	}

	/*
	 * Handle end-of-iteration, then capture the value we will return after
	 * advancing the iterator.
	 */

	if (i->exhausted) {
		free(i);
		*it = NULL;
		return NULL;
	}
	ret = i->ent;

	/*
	 * One iteration cycle.  Exhaust the current idx: moving on to the next
	 * restarts the bucket and ent subloops.
	 */
	for (; i->idx < i->htab->size; i->idx++, i->bucket = NULL, i->ent = NULL) {

		if (i->htab->tab[i->idx] == NULL)
			continue;
		if (i->bucket == NULL)
			i->bucket = i->htab->tab[i->idx];

		/*
		 * Check the current bucket: moving on to the next restarts the
		 * ent subloop.
		 */
		for (; i->bucket; i->bucket = i->bucket->next, i->ent = NULL) {

			if (i->ent == NULL)
				i->ent = i->bucket->head;
			else
				i->ent = i->htab->ops->next(i->ent);
			if (i->ent)
				return ret;
		}
	}

	/*
	 * End of advancement, but that doesn't mean we can't return.
	 */
	i->exhausted = 1;
	return ret;
}

void
dt_htab_next_destroy(dt_htab_next_t *i)
{
	free(i);
}

/*
 * Return the number of entries in the hashtable.
 */
size_t dt_htab_entries(const dt_htab_t *htab)
{
	return htab->nentries;
}

/*
 * Report statistics on the given hashtable.
 */
void dt_htab_stats(const char *name, const dt_htab_t *htab)
{
	int	i;
	int	slotc = 0;
	int	bckc = 0;
	int	maxbckinslot = 0;
	int	maxentinbck = 0;
	int	maxentinslot = 0;
	int	bckinslot = 0;
	int	entinbck = 0;
	int	entinslot = 0;

	for (i = 0; i < htab->size; i++) {
		dt_hbucket_t	*bucket = htab->tab[i];
		int		bucketc = 0;
		int		entryc = 0;

		if (!bucket)
			continue;

		slotc++;

		for (; bucket; bucket = bucket->next) {
			bucketc++;
			entryc += bucket->nentries;

			if (bucket->nentries > maxentinbck)
				maxentinbck = bucket->nentries;
		}

		if (bucketc > maxbckinslot)
			maxbckinslot = bucketc;
		if (entryc > maxentinslot)
			maxentinslot = entryc;

		bckinslot += bucketc;
		entinslot += entryc;
		entinbck += entryc;
		bckc += bucketc;
	}

	if (!slotc) {
		fprintf(stderr, "HSTAT %s - empty\n", name);
		return;
	}

	fprintf(stderr, "HSTAT %s - %d slots (%d used), %d buckets\n",
		name, htab->size, slotc, htab->nbuckets);
	fprintf(stderr,
		"HSTAT %s - avg: %d bck / slot, %d ent / bck, %d ent / slot\n",
		name, bckinslot / slotc, entinbck / bckc, entinslot / slotc);
	fprintf(stderr,
		"HSTAT %s - max: %d bck / slot, %d ent / bck, %d ent / slot\n",
		name, maxbckinslot, maxentinbck, maxentinslot);
}
