/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <dt_rodata.h>
#include <dt_string.h>
#include <dt_impl.h>

typedef struct dt_rohash	dt_rohash_t;
struct dt_rohash {
	const char	*data;		/* pointer to actual data */
	size_t		buf;		/* index of data buffer */
	size_t		off;		/* offset in bytes of this item */
	size_t		len;		/* length in bytes of this item */
	dt_rohash_t	*next;		/* next item in hash chain */
};

struct dt_rodata {
	dt_rohash_t	**hash;		/* array of hash buckets */
	size_t		hashsz;		/* size of hash bucket array */
	char		**bufs;		/* array of buffer pointers */
	char		*ptr;		/* pointer to current buffer location */
	size_t		nbufs;		/* size of buffer pointer array */
	size_t		bufsz;		/* size of individual buffer */
	size_t		size;		/* total size of items in bytes */
	size_t		item_size;	/* maximum item size */
};

/*
 * Grow the underlying storage by adding another fixed-size buffer to the
 * list.
 */
static int
dt_rodata_grow(dt_rodata_t *dp)
{
	char	*ptr, **bufs;

	ptr = malloc(dp->bufsz);
	if (ptr == NULL)
		return -1;

	bufs = realloc(dp->bufs, (dp->nbufs + 1) * sizeof(char *));
	if (bufs == NULL) {
		free(ptr);
		return -1;
	}

	dp->nbufs++;
	dp->bufs = bufs;
	dp->ptr = ptr;
	dp->bufs[dp->nbufs - 1] = dp->ptr;

	return 0;
}

/*
 * Create a data structure to hold read-only data.  The underlying storage is
 * structured as a list of fixed-size buffers.  We also maintain a hashtable
 * with entries for every element added to the read-only data storage.  This
 * is mostly used to aim fast lookup of elements and (where applicable) to
 * provide duplicate-avoidance.
 */
dt_rodata_t *
dt_rodata_create(size_t bufsz)
{
	dt_rodata_t	*dp = malloc(sizeof(dt_rodata_t));
	size_t		nbuckets = _dtrace_strbuckets;

	assert(bufsz != 0);

	if (dp == NULL)
		return NULL;

	memset(dp, 0, sizeof(dt_rodata_t));
	dp->hash = malloc(nbuckets * sizeof(dt_rohash_t *));
	if (dp->hash == NULL)
		goto err;

	memset(dp->hash, 0, nbuckets * sizeof(dt_rohash_t *));
	dp->hashsz = nbuckets;
	dp->bufs = NULL;
	dp->ptr = NULL;
	dp->nbufs = 0;
	dp->bufsz = bufsz;

	if (dt_rodata_grow(dp) == -1)
		goto err;

	/*
	 * Pre-populate the read-only data with a 0-byte at offset 0 so that a
	 * NULL pointer can be mapped to offset 0.
	 */
	*dp->ptr++ = 0;
	dp->size = 1;

	return dp;

err:
	dt_rodata_destroy(dp);
	return NULL;
}

void
dt_rodata_destroy(dt_rodata_t *dp)
{
	dt_rohash_t	*hp, *hq;
	size_t		i;

	if (dp == NULL)
		return;

	for (i = 0; i < dp->hashsz; i++) {
		for (hp = dp->hash[i]; hp != NULL; hp = hq) {
			hq = hp->next;
			free(hp);
		}
	}

	for (i = 0; i < dp->nbufs; i++)
		free(dp->bufs[i]);

	if (dp->hash != NULL)
		free(dp->hash);
	if (dp->bufs != NULL)
		free(dp->bufs);

	free(dp);
}

static int
dt_rodata_compare(dt_rodata_t *dp, dt_rohash_t *hp, const char *ptr, size_t len)
{
	size_t		b = hp->buf;
	const char	*buf = hp->data;

	while (len != 0) {
		size_t	resid, n;
		int	rv;

		if (buf == dp->bufs[b] + dp->bufsz)
			buf = dp->bufs[++b];

		resid = dp->bufs[b] + dp->bufsz - buf;
		n = MIN(resid, len);

		if ((rv = memcmp(buf, ptr, n)) != 0)
			return rv;

		buf += n;
		ptr += n;
		len -= n;
	}

	return 0;
}

/*
 * Copy-in a block of data, without adding any entries to the hashtable.  This
 * can be used to consolidate read-only data blocks from multiple sources where
 * de-duplication and lookup are not needed or wanted.
 */
static int
dt_rodata_copyin(dt_rodata_t *dp, const char *ptr, size_t len)
{
	char	*old_p = dp->ptr;
	size_t	old_n = dp->nbufs;
	size_t	b = dp->nbufs - 1;

	while (len != 0) {
		size_t	resid, n;

		if (dp->ptr == dp->bufs[b] + dp->bufsz) {
			if (dt_rodata_grow(dp) == -1)
				goto err;
			b++;
		}

		resid = dp->bufs[b] + dp->bufsz - dp->ptr;
		n = MIN(resid, len);
		memcpy(dp->ptr, ptr, n);

		dp->ptr += n;
		ptr += n;
		len -= n;
	}

	return 0;

err:
	while (dp->nbufs != old_n)
		free(dp->bufs[--dp->nbufs]);

	dp->ptr = old_p;
	return -1;
}

/*
 * Determine the offset of a data item, providing the hash value for the item.
 * Return -1 if the item is not found.
 */
static ssize_t
dt_rodata_xindex(dt_rodata_t *dp, const char *ptr, size_t len, uint32_t h)
{
	dt_rohash_t	*hp;

	for (hp = dp->hash[h]; hp != NULL; hp = hp->next) {
		if (dt_rodata_compare(dp, hp, ptr, len) == 0)
			return hp->off;
	}

	return -1;
}

/*
 * Determine the offset of a data item (or -1 if not found).
 */
ssize_t
dt_rodata_index(dt_rodata_t *dp, const char *ptr, size_t len)
{
	ssize_t		rc;
	uint32_t	h;

	if (ptr == NULL)
		return 0;	/* NULL maps to offset 0. */

	h = dt_gen_hval(ptr, len, len) % dp->hashsz;
	rc = dt_rodata_xindex(dp, ptr, len, h);

	return rc;
}

/*
 * Insert a new data item into the read-only data.
 */
ssize_t
dt_rodata_insert(dt_rodata_t *dp, const char *ptr, size_t len)
{
	dt_rohash_t	*hp;
	ssize_t		off;
	uint32_t	h;

	if (ptr == NULL)
		return 0;	/* NULL maps to offset 0. */

	h = dt_gen_hval(ptr, len, len) % dp->hashsz;
	off = dt_rodata_xindex(dp, ptr, len, h);
	if (off != -1)
		return off;

	/*
	 * Create a new hash bucket, initialize it, and insert it at the front
	 * of the hash chain for the appropriate bucket.
	 */
	if ((hp = malloc(sizeof(dt_rohash_t))) == NULL)
		return -1L;

	hp->data = dp->ptr;
	hp->buf = dp->nbufs - 1;
	hp->off = dp->size;
	hp->len = len;
	hp->next = dp->hash[h];

	/*
	 * Now copy the data into our buffer list, and then update the global
	 * counts of items and bytes.  Return the item's byte offset.
	 */
	if (dt_rodata_copyin(dp, ptr, len) == -1) {
		free(hp);
		return -1L;
	}

	dp->size += len;
	dp->hash[h] = hp;
	if (dp->item_size < len)
		dp->item_size = len;

	return hp->off;
}

/*
 * Return the total size of the read-only data.
 */
size_t
dt_rodata_size(const dt_rodata_t *dp)
{
	return dp->size;
}

/*
 * Return the size of the largest read-only data item.
 */
size_t
dt_rodata_max_item_size(const dt_rodata_t *dp)
{
	return dp->item_size;
}

/*
 * Copy data from the compile-time read-only data into a DIFO-style read-only
 * storage memory block.
 */
ssize_t
dt_rodata_copy(const char *s, size_t n, size_t off, char *buf)
{
	memcpy(buf + off, s, n);
	return n;
}

/*
 * Emit the data from the underlying storage by passing it to the provided
 * callback function.  The callback function will be call for each data chunk
 * in the underlying storage.
 */
ssize_t
dt_rodata_write(const dt_rodata_t *dp, dt_rodata_copy_f *func, void *private)
{
	ssize_t	res, total = 0;
	size_t	i;
	size_t	n;

	for (i = 0; i < dp->nbufs; i++, total += res) {
		if (i == dp->nbufs - 1)
			n = dp->ptr - dp->bufs[i];
		else
			n = dp->bufsz;

		if ((res = func(dp->bufs[i], n, total, private)) <= 0)
			break;
	}

	if (total == 0 && dp->size != 0)
		return -1;

	return total;
}
