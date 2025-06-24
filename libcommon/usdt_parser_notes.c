/*
 * Oracle Linux DTrace; USDT definitions parser - ELF notes.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gelf.h>

#include <dt_htab.h>
#include <sys/usdt_note_defs.h>

#include "usdt_parser.h"

static void dt_dbg_usdt(const char *fmt, ...)
{
#ifdef USDT_DEBUG
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
#endif
}

#define IS_ALIGNED(x, a)	(((x) & ((typeof(x))(a) - 1)) == 0)
#define ALIGN(x, a)		(((x) + ((a) - 1)) & ~((a) - 1))

typedef struct usdt_note {
	GElf_Nhdr	*hdr;
	const char	*name;
	const char	*desc;
} usdt_note_t;

static ssize_t
get_note(int out, usdt_data_t *data, ssize_t off, usdt_note_t *note)
{
	size_t		sz;

	assert(note != NULL);

	/* Validate the offset. */
	if (off >= data->size || sizeof(GElf_Nhdr) > data->size - off) {
		usdt_error(out, EINVAL, "Invalid ELF note offset %zi", off);
		return -1;
	}

	memset(note, 0, sizeof(usdt_note_t));

	/* Get note header and validate its alignment. */
	note->hdr = (GElf_Nhdr *)((char *)data->buf + off);
	off += sizeof(GElf_Nhdr);
	if (!IS_ALIGNED((uintptr_t)note->hdr, 4)) {
		usdt_error(out, EINVAL, "Pointer to note header not aligned");
		return -1;
	}

	dt_dbg_usdt("ELF note header { type %d, namesz %d, descsz %d }...\n",
		    note->hdr->n_type, note->hdr->n_namesz,
		    note->hdr->n_descsz);

	/* Validate the name offset and size. */
	sz = note->hdr->n_namesz;
	if (off >= data->size || sz > data->size - off) {
		usdt_error(out, EINVAL, "Invalid name size %d", sz);
		return -1;
	}

	note->name = (char *)data->buf + off;
	off += ALIGN(sz, 4);

	dt_dbg_usdt("ELF note '%s' (%d bytes)\n",
		    note->name, note->hdr->n_descsz);

	/* Validate the desc offset and size. */
	sz = note->hdr->n_descsz;
	if (off >= data->size || sz > data->size - off) {
		usdt_error(out, EINVAL, "Invalid desc size %d", sz);
		return -1;
	}

	note->desc = (char *)data->buf + off;
	off += ALIGN(sz, 4);

	/*
	 * If the offset reaches the end of the notes section, report this is
	 * as the last note.
	 */
	if (off >= data->size)
		return 0;

	return off;
}

typedef struct dt_provider	dt_provider_t;
typedef struct dt_probe		dt_probe_t;

/*
 * Defined providers are stored in a hashtable indexed on provider name.  The
 * probes defined in the provider are stored in pmap hashtable of the provider,
 * with a NULL function name.  These probes are used to validate tracepoints
 * that are found in the actual code.
 *
 * During tracepoint validation, tracepoint probes (with actual fuction names)
 * will be moved from the prbmap hashtable to the pvp->pmap hashtable.  These
 * probes have tracepoint data associated with them, and are the probes that
 * will be emitted as parsed data for the provider.  Any probes that do not
 * have tracepoints will be ignored.
 *
 * The pvp->pmap hashtable therefore will require a specific cleanup function
 * to ensure that the probe data is freed.
 *
 * The dt_provider_t.probec tracks the number of probes with tracepoints.
 */
struct dt_provider {
	dt_hentry_t	he;
	const char	*name;			/* provider name */
	uint32_t	pattr;			/* provider attributes */
	uint32_t	mattr;			/* module attributes */
	uint32_t	fattr;			/* function attributes */
	uint32_t	nattr;			/* probe name attributes */
	uint32_t	aattr;			/* argument attributes */
	uint32_t	probec;			/* probe count */
	dt_htab_t	*pmap;			/* probe hash */
};

struct dt_probe {
	dt_hentry_t	he;
	dt_probe_t	*next;			/* next probe in list */
	const char	*prv;			/* provider name */
	const char	*mod;			/* module name */
	const char	*fun;			/* function name (or NULL) */
	const char	*prb;			/* probe name */
	uint32_t	ntp;			/* number of tracepoints */
	uint32_t	off;			/* tracepoint offset */
	uint8_t		is_enabled;		/* is-enabled probe (boolean) */
	uint8_t		nargc;			/* native argument count */
	uint8_t		xargc;			/* translated argument count */
	uint8_t		sargc;			/* source argument count */
	const char	*nargs;			/* native argument types */
	size_t		nargsz;			/* size of native arg types */
	char		*xargs;			/* translated argument types */
	size_t		xargsz;			/* size of xlated arg types */
	uint8_t		*xmap;			/* translated argument map */
	const char	*sargs;			/* source argument strings */
};

static dt_htab_t	*prvmap;
static dt_htab_t	*prbmap;

extern uint32_t str2hval(const char *, uint32_t);

static uint32_t prv_hval(const dt_provider_t *pvp) {
	return str2hval(pvp->name, 0);
}

static int prv_cmp(const dt_provider_t *p, const dt_provider_t *q) {
	return strcmp(p->name, q->name);
}

DEFINE_HE_STD_LINK_FUNCS(prv, dt_provider_t, he)

/*
 * Hashtable element cleanup function for providers.  It ensures that the probe
 * hashtable (pvp->pmap) is destroyed.
 */
static void *
prv_del_prov(dt_provider_t *head, dt_provider_t *pvp)
{
	head = prv_del(head, pvp);

	dt_htab_destroy(pvp->pmap);
	free(pvp);

	return head;
}

static dt_htab_ops_t prv_htab_ops = {
        .hval = (htab_hval_fn)prv_hval,
        .cmp = (htab_cmp_fn)prv_cmp,
        .add = (htab_add_fn)prv_add,
        .del = (htab_del_fn)prv_del_prov,
        .next = (htab_next_fn)prv_next
};

static uint32_t prb_hval(const dt_probe_t *prp) {
	uint32_t	hval;

	hval = str2hval(prp->prv, prp->off);
	hval = str2hval(prp->mod, hval);
	hval = str2hval(prp->fun, hval);

	return str2hval(prp->prb, hval);
}

static int prb_cmp(const dt_probe_t *p, const dt_probe_t *q) {
	int	rc;

	rc = strcmp(p->prv, q->prv);
	if (rc != 0)
		return rc;

	if (p->fun != NULL) {
		if (q->fun == NULL)
			return 1;
		else {
			rc = strcmp(p->fun, q->fun);
			if (rc != 0)
				return rc;
		}
	} else if (q->fun != NULL)
		return -1;

	rc = strcmp(p->prb, q->prb);
	if (rc != 0)
		return rc;

	/* Only compare offsets when both are not zero. */
	if (p->off == 0 || q->off == 0)

		return 0;
	return p->off - q->off;
}

DEFINE_HE_STD_LINK_FUNCS(prb, dt_probe_t, he)
DEFINE_HTAB_STD_OPS(prb)

/*
 * Probe hashtable element cleanup function to ensure that probe data is freed
 * when probes are removed from the pvp->pmap hashtable.  Note that probes in
 * the prbmap hashtable do *not* get freed upon removal because they get moved
 * to a pvp->pmap hashtable, and they will get freed when removed from there.
 */
static void *
prb_del_probe(dt_probe_t *head, dt_probe_t *prp)
{
	head = prb_del(head, prp);

	/*
	 * If this is not a function-specific probe (from a prov note), free
	 * the translated arg data and the probe itself.
	 * If this is a function-specific probe (from a usdt note), walk the
	 * list of tracepoint probe, freeing each probe in the list.
	 */
	if (prp->fun == NULL) {
		free(prp->xargs);
		free(prp->xmap);
		free(prp);
	} else {
		dt_probe_t	*nxt;

		do {
			nxt = prp->next;
			free(prp);
		} while ((prp = nxt) != NULL);
	}

	return head;
}

static dt_htab_ops_t pmap_htab_ops = {
        .hval = (htab_hval_fn)prb_hval,
        .cmp = (htab_cmp_fn)prb_cmp,
        .add = (htab_add_fn)prb_add,
        .del = (htab_del_fn)prb_del_probe,
        .next = (htab_next_fn)prb_next
};

/*
 * Return the cummulative string length of 'cnt' consecutive 0-terminated
 * strings.  If skip > 0, it indicates how many extra bytes are to be skipped
 * after the 0-byte at the end of each string.
 * Return -1 if end is reached before 'cnt' strings were found.
 */
static ssize_t
strarray_size(uint8_t cnt, const char *str, const char *end, size_t skip)
{
	const char	*p = str;

	while (cnt-- > 0) {
		if (p >= end)
			return -1;

		p += strlen(p) + 1 + skip;
	}

	return p - str;
}

static int
parse_prov_note(int out, dof_helper_t *dhp, usdt_data_t *data,
		usdt_note_t *note)
{
	const char	*p = note->desc;
	dt_provider_t	prvt, *pvp;
	const uint32_t	*vals;
	uint32_t	probec;
	int		i;

	prvt.name = p;
	p += ALIGN(strlen(p) + 1, 4);
	if (p + 6 * sizeof(uint32_t) - note->desc > note->hdr->n_descsz) {
		usdt_error(out, EINVAL, "Incomplete note data");
		return -1;
	}

	if ((pvp = dt_htab_lookup(prvmap, &prvt)) == NULL) {
		if ((pvp =  malloc(sizeof(dt_provider_t))) == NULL) {
			usdt_error(out, ENOMEM, "Failed to allocate provider");
			return -1;
		}
		memset(pvp, 0, sizeof(dt_provider_t));
		pvp->name = prvt.name;
		dt_htab_insert(prvmap, pvp);
		pvp->pmap = dt_htab_create(&pmap_htab_ops);
	} else {
		usdt_error(out, EEXIST, "Duplicate provider: %s", prvt.name);
		return -1;
	}

	vals = (uint32_t *)p;
	pvp->pattr = *vals++;
	pvp->mattr = *vals++;
	pvp->fattr = *vals++;
	pvp->nattr = *vals++;
	pvp->aattr = *vals++;
	probec = *vals++;

	dt_dbg_usdt("[prov] %s::: with %d probe%s\n", pvp->name, probec,
		    probec == 1 ? "" : "s");

	p = (char *)vals;
	for (i = 0; i < probec; i++) {
		int		argc;
		dt_probe_t	prbt, *prp;
		ssize_t		len;

		p = (const char *)ALIGN((uintptr_t)p, 4);
		prbt.prv = pvp->name;
		prbt.mod = dhp->dofhp_mod;
		prbt.fun = NULL;
		prbt.prb = p;
		prbt.off = 0;
		p += strlen(p) + 1;
		if (p + 2 * sizeof(uint8_t) - note->desc > note->hdr->n_descsz) {
			usdt_error(out, EINVAL, "Incomplete note data");
			return -1;
		}

		if ((prp = dt_htab_lookup(pvp->pmap, &prbt)) == NULL) {
			if ((prp = malloc(sizeof(dt_probe_t))) == NULL) {
				usdt_error(out, ENOMEM, "Failed to allocate probe");
				return -1;
			}
			memset(prp, 0, sizeof(dt_probe_t));
			prp->prv = prbt.prv;
			prp->mod = prbt.mod;
			prp->prb = prbt.prb;
			prp->off = 0;
			dt_htab_insert(pvp->pmap, prp);
		} else {
			usdt_error(out, EEXIST, "Duplicate probe: %s:%s::%s",
				   prbt.prv, prbt.mod, prbt.prb);
			return -1;
		}

		prp->next = NULL;
		prp->ntp = 0;
		prp->is_enabled = 0;
		prp->nargc = argc = *(uint8_t *)p++;
		len = strarray_size(argc, p, note->desc + note->hdr->n_descsz,
				    0);
		if (len == -1) {
			usdt_error(out, EINVAL, "Incomplete note data");
			return -1;
		}
		prp->nargsz = len;
		prp->nargs = p;

		p += len;
		if (p - note->desc > note->hdr->n_descsz) {
			usdt_error(out, EINVAL, "Incomplete note data");
			return -1;
		}

		prp->xargc = argc = *(uint8_t *)p++;
		len = strarray_size(argc, p, note->desc + note->hdr->n_descsz,
				    1);
		if (len == -1) {
			usdt_error(out, EINVAL, "Incomplete note data");
			return -1;
		} else if (len > 0) {
			int	j;
			char	*q;

			len -= argc;
			prp->xargsz = len;
			prp->xargs = q = malloc(len);
			prp->xmap = malloc(argc * sizeof(uint8_t));
			if (prp->xargs == NULL || prp->xmap == NULL) {
				usdt_error(out, ENOMEM, "Failed to allocate memory");
				return -1;
			}
			for (j = 0; j < argc; j++) {
				q = stpcpy(q, p);
				q++;
				p += strlen(p) + 1;
				prp->xmap[j] = *p;
				p++;
			}
		} else {
			prp->xargsz = 0;
			prp->xargs = NULL;
		}

		dt_dbg_usdt("[prov]   %s:%s::%s (nargc %d, xargc %d)\n",
			    prp->prv, prp->mod, prp->prb, prp->nargc,
			    prp->xargc);
	}

	return 0;
}

static int
parse_usdt_note(int out, dof_helper_t *dhp, usdt_data_t *data,
		usdt_note_t *note)
{
	const char	*p = note->desc;
	uint64_t	off, fno;
	dt_probe_t	prbt, *prp;

	data = data->next;
	if (data == NULL) {
		usdt_error(out, EINVAL, "Missing .rodata data");
		return -1;
	}

	if (p + 2 * sizeof(uint64_t) - note->desc > note->hdr->n_descsz) {
		usdt_error(out, EINVAL, "Incomplete note data");
		return -1;
	}

	off = *(uint64_t *)p;
	p += sizeof(uint64_t);
	fno = *(uint64_t *)p;
	p += sizeof(uint64_t);

	prbt.prv = p;
	p += strlen(p) + 1;
	if (p - note->desc > note->hdr->n_descsz) {
		usdt_error(out, EINVAL, "Incomplete note data");
		return -1;
	}
	prbt.mod = dhp->dofhp_mod;
	if (fno < data->base || (fno -= data->base) >= data->size) {
		usdt_error(out, EINVAL, "Invalid function name offset");
		return -1;
	}
	prbt.fun = (char *)data->buf + fno;
	prbt.prb = p;
	p += strlen(p) + 1;
	if (p - note->desc > note->hdr->n_descsz) {
		usdt_error(out, EINVAL, "Incomplete note data");
		return -1;
	}
	prbt.off = off;

	/*
	 * If the probe name has encoded hyphens, perform in-place changing
	 * from "__" into "-".
	 */
	if (strstr(prbt.prb, "__") != NULL) {
		char		*q;
		const char	*s = prbt.prb, *e = p;

		for (q = (char *)s; s < e; s++, q++) {
			if (s[0] == '_' && s[1] == '_') {
				*q = '-';
				s++;
			} else if (s > q)
				*q = *s;
		}
	}

	if ((prp = dt_htab_lookup(prbmap, &prbt)) == NULL) {
		if ((prp = malloc(sizeof(dt_probe_t))) == NULL) {
			usdt_error(out, ENOMEM, "Failed to allocate probe");
			return -1;
		}
		memset(prp, 0, sizeof(dt_probe_t));
		prp->prv = prbt.prv;
		prp->mod = prbt.mod;
		prp->fun = prbt.fun;
		prp->prb = prbt.prb;
		prp->off = prbt.off;
		dt_htab_insert(prbmap, prp);
	} else {
		usdt_error(out, EEXIST, "Duplicate probe: %s:%s:%s:%s",
			   prbt.prv, prbt.mod, prbt.fun, prbt.prb);
		return -1;
	}

	prp->next = NULL;
	prp->is_enabled = (note->hdr->n_type == _USDT_EN_NOTE_TYPE ? 1 : 0);
	prp->ntp = 0;
	prp->sargc = *p++;
	prp->sargs = p;
	p += strlen(p) + 1;
	if (p - note->desc > note->hdr->n_descsz) {
		usdt_error(out, EINVAL, "Incomplete note data");
		return -1;
	}

	dt_dbg_usdt("[usdt]   %s:%s:%s:%s (nargc %d, offset %lx)\n",
		    prp->prv, prp->mod, prp->fun, prp->prb, prp->nargc,
		    prp->off);

	return 0;
}

/*
 * Allocate a dof_parsed_t message structure of the given 'type', with 'len'
 * extra space following the structure.  The caller is responsible for calling
 * free on the returned value.
 * Return NULL if memory allocation failed (an error will have been emitted).
 */
static dof_parsed_t *
alloc_msg(int out, dof_parsed_info_t type, size_t len)
{
	dof_parsed_t	*msg;

	switch (type) {
	case DIT_PROVIDER:
		len += offsetof(dof_parsed_t, provider.name);
		break;
	case DIT_PROBE:
		len += offsetof(dof_parsed_t, probe.name);
		break;
	case DIT_ARGS_NATIVE:
		len += offsetof(dof_parsed_t, nargs.args);
		break;
	case DIT_ARGS_XLAT:
		len += offsetof(dof_parsed_t, xargs.args);
		break;
	case DIT_ARGS_MAP:
		len += offsetof(dof_parsed_t, argmap.argmap);
		break;
	case DIT_TRACEPOINT:
		len += offsetof(dof_parsed_t, tracepoint.args);
		break;
	default:
		usdt_error(out, EINVAL, "Unknown dof_parsed_t type: %d", type);
		return NULL;
	}

	msg = malloc(len);
	if (msg == NULL) {
		usdt_error(out, ENOMEM, "Failed to allocate msg (type %d, size %ld)",
			   type, len);
		return NULL;
	}
	memset(msg, 0, len);

	msg->size = len;
	msg->type = type;

	return msg;
}

static int 
emit_tp(int out, const dof_helper_t *dhp, const dt_probe_t *prp)
{
	dof_parsed_t	*msg;

	if ((msg = alloc_msg(out, DIT_TRACEPOINT, strlen(prp->sargs) + 1)) == NULL)
		return -1;

	msg->tracepoint.addr = prp->off + dhp->dofhp_addr;
	msg->tracepoint.is_enabled = prp->is_enabled;
	strcpy(msg->tracepoint.args, prp->sargs);

	usdt_parser_write_one(out, msg, msg->size);

	free(msg);

	dt_dbg_usdt("        Tracepoint at 0x%lx (0x%llx + 0x%x)%s\n",
		    prp->off + dhp->dofhp_addr, dhp->dofhp_addr, prp->off,
		    prp->is_enabled ? " (is_enabled)" : "");

	return 0;
}

static int
emit_probe(int out, const dof_helper_t *dhp, const dt_probe_t *prp)
{
	dof_parsed_t	*msg;
	char		*p;

	if ((msg = alloc_msg(out, DIT_PROBE, strlen(prp->mod) + 1 +
					     strlen(prp->fun) + 1 +
					     strlen(prp->prb) + 1)) == NULL)
		return -1;

	msg->probe.ntp = prp->ntp;
	msg->probe.nargc = prp->nargc;
	msg->probe.xargc = prp->xargc;

	p = stpcpy(msg->probe.name, prp->mod);
	p++;
	p = stpcpy(p, prp->fun);
	p++;
	strcpy(p, prp->prb);

	usdt_parser_write_one(out, msg, msg->size);

	free(msg);

	dt_dbg_usdt("      Probe %s:%s:%s:%s (%d tracepoints)\n",
		    prp->prv, prp->mod, prp->fun, prp->prb, prp->ntp);

	/* Emit native and translated arg type data (if any). */
	if (prp->nargc) {
		if ((msg = alloc_msg(out, DIT_ARGS_NATIVE, prp->nargsz)) == NULL)
			return -1;

		memcpy(msg->nargs.args, prp->nargs, prp->nargsz);

		usdt_parser_write_one(out, msg, msg->size);

		free(msg);

		if (prp->xargc) {
			size_t	mapsz = prp->xargc * sizeof(uint8_t);

			if ((msg = alloc_msg(out, DIT_ARGS_XLAT, prp->xargsz)) == NULL)
				return -1;
	
			memcpy(msg->xargs.args, prp->xargs, prp->xargsz);
	
			usdt_parser_write_one(out, msg, msg->size);
	
			free(msg);

			if ((msg = alloc_msg(out, DIT_ARGS_MAP, mapsz)) == NULL)
				return -1;
	
			memcpy(msg->argmap.argmap, prp->xmap, mapsz);
	
			usdt_parser_write_one(out, msg, msg->size);
	
			free(msg);
		}
	}

	while (prp != NULL) {
		if (emit_tp(out, dhp, prp) == -1)
			return -1;

		prp = prp->next;
	}

	return 0;
}

static int
emit_provider(int out, const dof_helper_t *dhp, const dt_provider_t *pvp)
{
	dof_parsed_t	*msg;
	dt_htab_next_t	*prbit = NULL;
	dt_probe_t	*prp;

	if ((msg = alloc_msg(out, DIT_PROVIDER, strlen(pvp->name) + 1)) == NULL)
		return -1;

	strcpy(msg->provider.name, pvp->name);
	msg->provider.nprobes = pvp->probec;

	usdt_parser_write_one(out, msg, msg->size);

	free(msg);

	dt_dbg_usdt("    Provider %s (%d probes)\n", pvp->name, pvp->probec);

	while ((prp = dt_htab_next(pvp->pmap, &prbit)) != NULL) {
		if (prp->fun == NULL)
			continue;

		if (emit_probe(out, dhp, prp) == -1)
			return -1;

		prp = prp->next;
	}

	return 0;
}

int
usdt_parse_notes(int out, dof_helper_t *dhp, usdt_data_t *data)
{
	ssize_t		off = 0;
	int		rc = 0;
	usdt_note_t	note;
	dt_probe_t	*ptp;
	dt_htab_next_t	*prbit, *prvit;
	dt_provider_t	*pvp;

	/* Hash tables to hold provider and probe info. */
	prvmap = dt_htab_create(&prv_htab_ops);
	prbmap = dt_htab_create(&prb_htab_ops);

	/* Process all prov and usdt notes. */
	while ((off = get_note(out, data, off, &note)) >= 0) {
		rc = -1;
		if (strcmp(note.name, "prov") == 0)
			rc = parse_prov_note(out, dhp, data, &note);
		else if (strcmp(note.name, "usdt") == 0)
			rc = parse_usdt_note(out, dhp, data, &note);
		else if (strcmp(note.name, "dver") == 0 ||
			 strcmp(note.name, "utsn") == 0)
			rc = 0;			/* ignore */
		else
			usdt_error(out, EINVAL, "Unknown note: %s", note.name);

		if (rc == -1)
			goto err;		/* error emitted */

		if (off == 0)
			break;
	}

	/* Bail on error. */
	if (off == -1)
		goto err;

	/*
	 * Loop through all tracepoints (from usdt notes) and validate them
	 * against the registered providers and probes (from prov notes).
	 * Validated tracepoints are added to the provider.
	 */
	prbit = NULL;
	while ((ptp = dt_htab_next(prbmap, &prbit)) != NULL) {
		dt_provider_t	prvt, *pvp;
		dt_probe_t	prbt, *prp;

		prvt.name = ptp->prv;
		if ((pvp = dt_htab_lookup(prvmap, &prvt)) == NULL) {
			usdt_error(out, ENOENT, "No such provider: %s",
				   ptp->prv);
			goto err;
		}

		/*
		 * First try to find a matching probe that already has one or
		 * more tracepoints, i.e. a probe that matches the function
		 * name as well.
		 */
		prbt.prv = ptp->prv;
		prbt.mod = ptp->mod;
		prbt.fun = ptp->fun;
		prbt.prb = ptp->prb;
		prbt.off = 0;
		if ((prp = dt_htab_lookup(pvp->pmap, &prbt)) == NULL) {
			/*
			 * Not found - make sure there is a defined probe (with
			 * NULL function name) that matches.
			 */
			prbt.fun = NULL;
			if ((prp = dt_htab_lookup(pvp->pmap, &prbt)) == NULL) {
				usdt_error(out, ENOENT, "No such probe: %s:::%s",
					   ptp->prv, ptp->prb);
				goto err;
			}
		}

		if (ptp->sargc != prp->nargc &&
		    (!ptp->is_enabled || ptp->sargc != 1)) {
			usdt_error(out, EINVAL,
				   "%s:::%s%s prototype mismatch: "
				   "%hhd passed, %hhd expected",
				   ptp->prv, ptp->prb,
				   ptp->is_enabled ? " (is-enabled)" : "",
				   ptp->sargc,
				   ptp->is_enabled ? 1 : prp->nargc);
			goto err;
		}

		/*
		 * The tracepoint is valid.  Remove it from the prbmap htab and
		 * add it to the provider.
		 * If there was a matching function-specific probe, add the
		 * tracepoint probe to it.
		 * If there was no matching function-specific probe, add the
		 * tracepoint probe to the provider.
		 * In either cases, argument data is copied.
		 */
		dt_htab_delete(prbmap, ptp);
		if (prp->fun != NULL) {
			ptp->next = prp->next;
			prp->next = ptp;
			prp->ntp++;
		} else {
			dt_htab_insert(pvp->pmap, ptp);
			ptp->ntp = 1;
			pvp->probec++;
		}

		ptp->nargc = prp->nargc;
		ptp->nargs = prp->nargs;
		ptp->nargsz = prp->nargsz;
		ptp->xargc = prp->xargc;
		ptp->xargs = prp->xargs;
		ptp->xargsz = prp->xargsz;
		ptp->xmap = prp->xmap;
	}

	/* Emit any provider that has tracepoints. */
	prvit = NULL;
	while ((pvp = dt_htab_next(prvmap, &prvit)) != NULL) {
		if (pvp->probec > 0 && emit_provider(out, dhp, pvp) == -1)
			goto err;
	}

	goto out;

err:
	rc = -1;

out:
	/*
	 * All tracepoint probes in prbmap should have been removed during
	 * proessing.
	 */
	assert(dt_htab_entries(prbmap) == 0);

	dt_htab_destroy(prvmap);
	dt_htab_destroy(prbmap);

	return rc;
}
