/*
 * FILE:	dtrace_hash.c
 * DESCRIPTION:	Dynamic Tracing: probe hashing functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include "dtrace.h"

#define DTRACE_HASHSTR(hash, probe)	\
	dtrace_hash_str(*((char **)((uintptr_t)(probe) + (hash)->dth_stroffs)))
#define DTRACE_HASHEQ(hash, lhs, rhs)	\
	(strcmp(*((char **)((uintptr_t)(lhs) + (hash)->dth_stroffs)), \
		*((char **)((uintptr_t)(rhs) + (hash)->dth_stroffs))) == 0)

static uint_t dtrace_hash_str(char *p)
{
	uint_t	g;
	uint_t	hval = 0;

	while (*p) {
		hval = (hval << 4) + *p++;
		if ((g = (hval & 0xf0000000)) != 0)
			hval ^= g >> 24;

		hval &= ~g;
	}

	return hval;
}

dtrace_probe_t *dtrace_hash_lookup(dtrace_hash_t *hash,
				   dtrace_probe_t *template)
{
	int			hashval = DTRACE_HASHSTR(hash, template);
	int			ndx = hashval & hash->dth_mask;
	dtrace_hashbucket_t	*bucket = hash->dth_tab[ndx];

	for (; bucket != NULL; bucket = bucket->dthb_next) {
		if (DTRACE_HASHEQ(hash, bucket->dthb_chain, template))
			return bucket->dthb_chain;
	}

	return NULL;
}

int dtrace_hash_collisions(dtrace_hash_t *hash, dtrace_probe_t *template)
{
	int			hashval = DTRACE_HASHSTR(hash, template);
	int			ndx = hashval & hash->dth_mask;
	dtrace_hashbucket_t	*bucket = hash->dth_tab[ndx];

	for (; bucket != NULL; bucket = bucket->dthb_next) {
		if (DTRACE_HASHEQ(hash, bucket->dthb_chain, template))
			return bucket->dthb_len;
	}

	return 0;
}
