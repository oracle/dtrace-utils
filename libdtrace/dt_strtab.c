/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <dt_strtab.h>
#include <dt_string.h>
#include <dt_impl.h>

typedef struct dt_strhash {
	const char *str_data;		/* pointer to actual string data */
	ulong_t str_buf;		/* index of string data buffer */
	size_t str_off;			/* offset in bytes of this string */
	size_t str_len;			/* length in bytes of this string */
	struct dt_strhash *str_next;	/* next string in hash chain */
} dt_strhash_t;

struct dt_strtab {
	dt_strhash_t **str_hash;	/* array of hash buckets */
	ulong_t str_hashsz;		/* size of hash bucket array */
	char **str_bufs;		/* array of buffer pointers */
	char *str_ptr;			/* pointer to current buffer location */
	ulong_t str_nbufs;		/* size of buffer pointer array */
	size_t str_bufsz;		/* size of individual buffer */
	ulong_t str_nstrs;		/* total number of strings in strtab */
	size_t str_size;		/* total size of strings in bytes */
};

static int
dt_strtab_grow(dt_strtab_t *sp)
{
	char *ptr, **bufs;

	if ((ptr = malloc(sp->str_bufsz)) == NULL)
		return -1;

	bufs = realloc(sp->str_bufs, (sp->str_nbufs + 1) * sizeof(char *));

	if (bufs == NULL) {
		free(ptr);
		return -1;
	}

	sp->str_nbufs++;
	sp->str_bufs = bufs;
	sp->str_ptr = ptr;
	sp->str_bufs[sp->str_nbufs - 1] = sp->str_ptr;

	return 0;
}

dt_strtab_t *
dt_strtab_create(size_t bufsz)
{
	dt_strtab_t	*sp = malloc(sizeof(dt_strtab_t));
	uint_t		nbuckets = _dtrace_strbuckets;

	assert(bufsz != 0);

	if (sp == NULL)
		return NULL;

	memset(sp, 0, sizeof(dt_strtab_t));
	sp->str_hash = malloc(nbuckets * sizeof(dt_strhash_t *));

	if (sp->str_hash == NULL)
		goto err;

	memset(sp->str_hash, 0, nbuckets * sizeof(dt_strhash_t *));
	sp->str_hashsz = nbuckets;
	sp->str_bufs = NULL;
	sp->str_ptr = NULL;
	sp->str_nbufs = 0;
	sp->str_bufsz = bufsz;
	sp->str_nstrs = 1;
	sp->str_size = 1;

	if (dt_strtab_grow(sp) == -1)
		goto err;

	/*
	 * Pre-populate the string table with the empty string as frist string,
	 * at offset 0.  We use this guarantee in dt_strtab_insert() and
	 * dt_strtab_index().
	 */
	*sp->str_ptr++ = '\0';
	sp->str_size = 1;
	sp->str_nstrs = 1;

	return sp;

err:
	dt_strtab_destroy(sp);
	return NULL;
}

void
dt_strtab_destroy(dt_strtab_t *sp)
{
	dt_strhash_t *hp, *hq;
	ulong_t i;

	if(sp == NULL)
		return;

	for (i = 0; i < sp->str_hashsz; i++) {
		for (hp = sp->str_hash[i]; hp != NULL; hp = hq) {
			hq = hp->str_next;
			free(hp);
		}
	}

	for (i = 0; i < sp->str_nbufs; i++)
		free(sp->str_bufs[i]);

	if (sp->str_hash != NULL)
		free(sp->str_hash);
	if (sp->str_bufs != NULL)
		free(sp->str_bufs);

	free(sp);
}

static int
dt_strtab_compare(dt_strtab_t *sp, dt_strhash_t *hp,
    const char *str, size_t len)
{
	ulong_t b = hp->str_buf;
	const char *buf = hp->str_data;
	size_t resid, n;
	int rv;

	while (len != 0) {
		if (buf == sp->str_bufs[b] + sp->str_bufsz)
			buf = sp->str_bufs[++b];

		resid = sp->str_bufs[b] + sp->str_bufsz - buf;
		n = MIN(resid, len);

		if ((rv = strncmp(buf, str, n)) != 0)
			return rv;

		buf += n;
		str += n;
		len -= n;
	}

	return 0;
}

static int
dt_strtab_copyin(dt_strtab_t *sp, const char *str, size_t len)
{
	char *old_p = sp->str_ptr;
	ulong_t old_n = sp->str_nbufs;

	ulong_t b = sp->str_nbufs - 1;
	size_t resid, n;

	while (len != 0) {
		if (sp->str_ptr == sp->str_bufs[b] + sp->str_bufsz) {
			if (dt_strtab_grow(sp) == -1)
				goto err;
			b++;
		}

		resid = sp->str_bufs[b] + sp->str_bufsz - sp->str_ptr;
		n = MIN(resid, len);
		memcpy(sp->str_ptr, str, n);

		sp->str_ptr += n;
		str += n;
		len -= n;
	}

	return 0;

err:
	while (sp->str_nbufs != old_n)
		free(sp->str_bufs[--sp->str_nbufs]);

	sp->str_ptr = old_p;
	return -1;
}

ssize_t
dt_strtab_xindex(dt_strtab_t *sp, const char *str, size_t len, ulong_t h)
{
	dt_strhash_t	*hp;

	for (hp = sp->str_hash[h]; hp != NULL; hp = hp->str_next) {
		if (dt_strtab_compare(sp, hp, str, len + 1) == 0)
			return hp->str_off;
	}

	return -1;
}

ssize_t
dt_strtab_index(dt_strtab_t *sp, const char *str)
{
	size_t	slen;
	ssize_t	rc;
	ulong_t	h;

	if (str == NULL || str[0] == '\0')
		return 0;	/* The empty string is always at offset 0. */

	slen = strlen(str);
	h = str2hval(str, slen) % sp->str_hashsz;
	rc = dt_strtab_xindex(sp, str, slen, h);

	return rc;
}

ssize_t
dt_strtab_insert(dt_strtab_t *sp, const char *str)
{
	dt_strhash_t	*hp;
	size_t		slen;
	ssize_t		off;
	ulong_t		h;

	if (str == NULL || str[0] == '\0')
		return 0;	/* The empty string is always at offset 0. */

	slen = strlen(str);
	h = str2hval(str, slen) % sp->str_hashsz;
	off = dt_strtab_xindex(sp, str, slen, h);
	if (off != -1)
		return off;

	/*
	 * Create a new hash bucket, initialize it, and insert it at the front
	 * of the hash chain for the appropriate bucket.
	 */
	if ((hp = malloc(sizeof(dt_strhash_t))) == NULL)
		return -1L;

	hp->str_data = sp->str_ptr;
	hp->str_buf = sp->str_nbufs - 1;
	hp->str_off = sp->str_size;
	hp->str_len = slen;
	hp->str_next = sp->str_hash[h];

	/*
	 * Now copy the string data into our buffer list, and then update
	 * the global counts of strings and bytes.  Return str's byte offset.
	 */
	if (dt_strtab_copyin(sp, str, slen + 1) == -1) {
		free(hp);
		return -1L;
	}

	sp->str_nstrs++;
	sp->str_size += slen + 1;
	sp->str_hash[h] = hp;

	return hp->str_off;
}

size_t
dt_strtab_size(const dt_strtab_t *sp)
{
	return sp->str_size;
}

/*
 * Copy strings from the compile-time string table into a DIFO-dtyle string
 * table storage memory block.
 */
ssize_t
dt_strtab_copystr(const char *s, size_t n, size_t off, char *buf)
{
	memcpy(buf + off, s, n);
	return n;
}

ssize_t
dt_strtab_write(const dt_strtab_t *sp, dt_strtab_write_f *func, void *private)
{
	ssize_t res, total = 0;
	ulong_t i;
	size_t n;

	for (i = 0; i < sp->str_nbufs; i++, total += res) {
		if (i == sp->str_nbufs - 1)
			n = sp->str_ptr - sp->str_bufs[i];
		else
			n = sp->str_bufsz;

		if ((res = func(sp->str_bufs[i], n, total, private)) <= 0)
			break;
	}

	if (total == 0 && sp->str_size != 0)
		return -1;

	return total;
}
