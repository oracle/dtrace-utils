/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <assert.h>
#include <byteswap.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <linux/btf.h>

#include <dt_impl.h>
#include <dt_btf.h>

typedef struct btf_header	btf_header_t;
typedef struct btf_type		btf_type_t;
typedef struct btf_enum		btf_enum_t;
typedef struct btf_array	btf_array_t;
typedef struct btf_member	btf_member_t;
typedef struct btf_param	btf_param_t;
typedef struct btf_var		btf_var_t;
typedef struct btf_var_secinfo	btf_var_secinfo_t;
typedef struct btf_decl_tag	btf_decl_tag_t;
typedef struct btf_enum64	btf_enum64_t;

struct dt_btf {
	void		*data;			/* raw BTF data */
	size_t		size;			/* raw BTF data size */
	btf_header_t	*hdr;			/* BTF header */
	char		*sdata;			/* string data */
	uint32_t	ssize;			/* string data size */
	int32_t		type_cnt;		/* number of types */
	btf_type_t	**types;		/* type table */
	ctf_id_t	*ctfids;		/* matching CTF type ids */
};

static const char *
dt_btf_errmsg(int err)
{
	return strerror(err);
}

static int
dt_btf_set_errno(dtrace_hdl_t *dtp, int err)
{
	dtp->dt_btferr = err;
	return -1;
}

static void *
dt_btf_set_load_errno(dtrace_hdl_t *dtp, int err)
{
	dtp->dt_btferr = err;
	return NULL;
}

static void
dt_btf_error(dtrace_hdl_t *dtp, int eid, const char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	dt_set_errmsg(dtp, dt_errtag(eid), NULL, NULL, 0, format, ap);
	dt_set_errno(dtp, EDT_BPF);
	va_end(ap);
}

static ctf_id_t
dt_ctf_set_errno(dtrace_hdl_t *dtp, int err)
{
	dtp->dt_ctferr = err;
	dt_set_errno(dtp, EDT_CTF);
	return CTF_ERR;
}

static ctf_id_t
dt_ctf_error(dtrace_hdl_t *dtp, ctf_dict_t *ctf)
{
	return dt_ctf_set_errno(dtp, ctf_errno(ctf));
}

static int
dt_btf_validate_header(dtrace_hdl_t *dtp, dt_btf_t *btf)
{
	btf_header_t	*hdr = (btf_header_t *)btf->data;

	btf->hdr = hdr;

	/* Validate magic number. */
	if (hdr->magic != BTF_MAGIC) {
		dt_dprintf("BTF magic mismatch (%04x, expected %04x)\n",
			   hdr->magic, BTF_MAGIC);
		return dt_btf_set_errno(dtp, EINVAL);
	}

	/* Validate header length. */
	if (hdr->hdr_len < sizeof(btf_header_t) ||
	    hdr->hdr_len >= btf->size) {
		dt_dprintf("BTF bad header length (%d)\n", hdr->hdr_len);
		return dt_btf_set_errno(dtp, EINVAL);
	}

	/* Validate data offsets and lengths. */
	if (hdr->type_off % 4) {
		dt_dprintf("BTF type offset misaligned (%d)\n", hdr->type_off);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (hdr->type_off + hdr->type_len > hdr->str_off) {
		dt_dprintf("BTF bad type section length (%d)\n", hdr->type_len);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (hdr->hdr_len + hdr->str_off + hdr->str_len > btf->size) {
		dt_dprintf("BTF bad string section length (%d)\n",
			   hdr->str_len);
		return dt_btf_set_errno(dtp, EINVAL);
	}

	return 0;
}

static size_t
dt_btf_type_size(const char *ptr)
{
	btf_type_t	*type = (btf_type_t *)ptr;
	size_t		size = sizeof(btf_type_t);
	int		vlen = BTF_INFO_VLEN(type->info);

	switch (BTF_INFO_KIND(type->info)) {
	case BTF_KIND_INT:
		return size + sizeof(uint32_t);
	case BTF_KIND_ARRAY:
		return size + sizeof(btf_array_t);
	case BTF_KIND_STRUCT:
	case BTF_KIND_UNION:
		return size + vlen * sizeof(btf_member_t);
	case BTF_KIND_ENUM:
		return size + vlen * sizeof(btf_enum_t);
	case BTF_KIND_FUNC_PROTO:
		return size + vlen * sizeof(btf_param_t);
	case BTF_KIND_VAR:
		return size + sizeof(btf_var_t);
	case BTF_KIND_DATASEC:
		return size + vlen * sizeof(btf_var_secinfo_t);
	case BTF_KIND_DECL_TAG:
		return size + sizeof(btf_decl_tag_t);
	case BTF_KIND_TYPE_TAG:
		return size;
	case BTF_KIND_ENUM64:
		return size + vlen * sizeof(btf_enum64_t);
	case BTF_KIND_PTR:
	case BTF_KIND_FWD:
	case BTF_KIND_TYPEDEF:
	case BTF_KIND_VOLATILE:
	case BTF_KIND_CONST:
	case BTF_KIND_RESTRICT:
	case BTF_KIND_FUNC:
	case BTF_KIND_FLOAT:
		return size;
	default:
		return -EINVAL;
	}

	return -EINVAL;
}

static int
dt_btf_decode(dtrace_hdl_t *dtp, dt_btf_t *btf)
{
	btf_header_t	*hdr;
	char		*ptr, *end;
	int32_t		idx;
	char		*tdata;

	if (dt_btf_validate_header(dtp, btf) == -1)
		return -1;

	hdr = btf->hdr;
	btf->sdata = (char *)btf->data + hdr->hdr_len + hdr->str_off;
	tdata = (char *)btf->data + hdr->hdr_len + hdr->type_off;

	/* First count how many types there really are. */
	ptr = tdata;
	end = tdata + hdr->type_len;
	idx = 1;
	while (ptr + sizeof(btf_type_t) <= end) {
		int		size = dt_btf_type_size(ptr);
		btf_type_t	*type = (btf_type_t *)ptr;

		if (size < 0)
			return dt_btf_set_errno(dtp,  -size);
		if (ptr + size > end)
			return dt_btf_set_errno(dtp,  EINVAL);
		if (dt_btf_get_string(dtp, btf, type->name_off) == NULL)
			return dt_btf_set_errno(dtp,  EINVAL);

		idx++;
		ptr += size;
	}

	/* Next, populate the type offsets table. */
	btf->type_cnt = idx;
	btf->types = dt_calloc(dtp, btf->type_cnt, sizeof(btf_type_t *));

	ptr = tdata;
	idx = 1;
	while (ptr + sizeof(btf_type_t) <= end) {
		int	size = dt_btf_type_size(ptr);

		btf->types[idx++] = (btf_type_t *)ptr;
		ptr += size;
	}

	return 0;
}

/*
 * Try to load BTF data from the given file.  We use FILE-based IO primitives
 * instead of fd-based IO primitives because the BTF data is usually provided
 * by sysfs pseudo-files and the fd-based IO primitives do not seem to work
 * with that.
 */
static dt_btf_t *
dt_btf_load(dtrace_hdl_t *dtp, const char *fn)
{
	FILE		*fp;
	dt_btf_t	*btf = NULL;
	btf_header_t	hdr;
	void		*data = NULL;
	Elf		*elf;
	size_t		shstrs;
	Elf_Scn		*sp = NULL;
	int		err = 0;

	btf = dt_zalloc(dtp, sizeof(dt_btf_t));
	if (btf == NULL)
		return dt_btf_set_load_errno(dtp, ENOMEM);

	fp = fopen(fn, "rb");
	if (fp == NULL)
		return dt_btf_set_load_errno(dtp,  errno);

	/* First see whether this might be a file with raw BTF data. */
	if (fread(&hdr, 1, sizeof(hdr), fp) < sizeof(hdr))
		goto fail;

	rewind(fp);
	if (hdr.magic == BTF_MAGIC) {
		struct stat	st;

		if (fstat(fileno(fp), &st) == -1)
			goto fail;

		data = dt_alloc(dtp, st.st_size);
		if (data == NULL)
			goto fail;

		if (fread(data, 1, st.st_size, fp) < st.st_size)
			goto fail;

		fclose(fp);

		btf->data = data;
		btf->size = st.st_size;
	} else {
		/* Next see whether this might be an ELF file with BTF data. */
		elf = elf_begin(fileno(fp), ELF_C_READ_MMAP, NULL);
		if (elf == NULL)
			goto elf_fail_no_end;
		if (elf_getshdrstrndx(elf, &shstrs) == -1)
			goto elf_fail;

		while ((sp = elf_nextscn(elf, sp)) != NULL) {
			GElf_Shdr	sh;
			Elf_Data	*edata;
			const char	*name;

			/* Skip malformed sections. */
			if (gelf_getshdr(sp, &sh) == NULL ||
			    sh.sh_type == SHT_NULL)
				continue;

			name = elf_strptr(elf, shstrs, sh.sh_name);
			if (name == NULL || strcmp(name, ".BTF") != 0)
				continue;

			/* Found the section we need. */
			edata = elf_getdata(sp, 0);
			if (edata == NULL)
				goto elf_fail;

			data = dt_alloc(dtp, edata->d_size);
			if (data == NULL)
				goto elf_fail;

			memcpy(data, edata->d_buf, edata->d_size);

			btf->data = data;
			btf->size = edata->d_size;

			elf_end(elf);
			fclose(fp);

			break;
		}
	}

	return btf;

elf_fail:
	elf_end(elf);

elf_fail_no_end:
	err = elf_errno();
	dt_btf_error(dtp, 0, "BTF: %s", elf_errmsg(err));
	fclose(fp);

	return NULL;

fail:
	dt_free(dtp, data);
	dt_btf_set_errno(dtp,  err ? err : errno);
	fclose(fp);

	return NULL;
}

dt_btf_t *
dt_btf_load_file(dtrace_hdl_t *dtp, const char *fn)
{
	dt_btf_t	*btf;

	btf = dt_btf_load(dtp, fn);
	if (btf == NULL) {
		dt_dprintf("Cannot open BTF file %s: %s\n", fn,
			   dt_btf_errmsg(dtp->dt_btferr));
		return NULL;
	}

	if (dt_btf_decode(dtp, btf) == -1) {
		dt_dprintf("Cannot decode BTF data %s: %s\n", fn,
			   dt_btf_errmsg(dtp->dt_btferr));
		return NULL;
	}

	return btf;
}

static ctf_id_t
dt_btf_add_to_ctf(dtrace_hdl_t *dtp, dt_btf_t *btf, ctf_dict_t *ctf,
		  int32_t type_id)
{
	btf_type_t	*type;
	int		kind, vlen;
	const char	*name;
	ctf_id_t	ctfid;
	ctf_encoding_t	enc;

	/*
	 * If we are not constructing shared_ctf, we may be looking for a type
	 * in shared_ctf.
	 */
	if (dtp->dt_btf && btf != dtp->dt_btf) {
		if (type_id < dtp->dt_btf->type_cnt)
			return dtp->dt_btf->ctfids[type_id];

		type_id -= dtp->dt_btf->type_cnt - 1;
	}

	assert(type_id < btf->type_cnt);

	if (btf->ctfids[type_id] != CTF_ERR)
		return btf->ctfids[type_id];

	assert(type_id > 0);

	type = btf->types[type_id];
	kind = BTF_INFO_KIND(type->info);
	vlen = BTF_INFO_VLEN(type->info);
	name = dt_btf_get_string(dtp, btf, type->name_off);

	if (name && name[0]) {
		char	n[DT_TYPE_NAMELEN];

		/* Do we already have this type? */
		switch (kind) {
		case BTF_KIND_ENUM:
		case BTF_KIND_ENUM64:
			if (snprintf(n, sizeof(n), "enum %s",
				     name == NULL ? "(anon)" : name) < 0)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);

			ctfid = ctf_lookup_by_name(ctf, n);
			break;
		case BTF_KIND_UNION:
			if (snprintf(n, sizeof(n), "union %s",
				     name == NULL ? "(anon)" : name) < 0)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);
	
			ctfid = ctf_lookup_by_name(ctf, n);
			break;
		case BTF_KIND_STRUCT:
			if (snprintf(n, sizeof(n), "struct %s",
				     name == NULL ? "(anon)" : name) < 0)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);
	
			ctfid = ctf_lookup_by_name(ctf, n);
			break;
		default:
			ctfid = ctf_lookup_by_name(ctf, name);
		}

		if (ctfid != CTF_ERR) {
			btf->ctfids[type_id] = ctfid;
			return ctfid;
		}
	}

	switch (kind) {
	case BTF_KIND_INT: {
		uint32_t	data = *(uint32_t *)(type + 1);
		uint32_t	benc = BTF_INT_ENCODING(data);

		enc.cte_format = 0;
		enc.cte_offset = BTF_INT_OFFSET(data);
		enc.cte_bits = BTF_INT_BITS(data);

		if (benc & BTF_INT_SIGNED)
			enc.cte_format |= CTF_INT_SIGNED;
		if (benc & BTF_INT_CHAR)
			enc.cte_format |= CTF_INT_CHAR;
		if (benc & BTF_INT_BOOL)
			enc.cte_format |= CTF_INT_BOOL;

		/* Some special case handling. */
		if (enc.cte_bits == 8) {
			if (strcmp(name, "_Bool") == 0)
				enc.cte_format = CTF_INT_BOOL;
			else
				enc.cte_format = CTF_INT_CHAR;
		}

		ctfid = ctf_add_integer(ctf, CTF_ADD_ROOT, name, &enc);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_FLOAT: {
		switch (type->size) {
		case 4:
			enc.cte_format = CTF_FP_SINGLE;
			break;
		case 8:
			enc.cte_format = CTF_FP_DOUBLE;
			break;
		case 16:
			enc.cte_format = CTF_FP_LDOUBLE;
			break;
		}

		enc.cte_offset = 0;
		enc.cte_bits = type->size * 8;

		ctfid = ctf_add_float(ctf, CTF_ADD_ROOT, name, &enc);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_ENUM: {
		int		i;
		btf_enum_t	*item = (btf_enum_t *)(type + 1);

		ctfid = ctf_add_enum(ctf, CTF_ADD_ROOT, name);
		if (ctfid == CTF_ERR)
			return dt_ctf_error(dtp, ctf);

		for (i = 0; i < vlen; i++, item++) {
			int	err;
			const char	*iname;

			iname = dt_btf_get_string(dtp, btf, item->name_off);
			err = ctf_add_enumerator(ctf, ctfid, iname, item->val);
			if (err == CTF_ERR)
				return dt_ctf_error(dtp, ctf);
		}

		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_PTR: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_pointer(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_CONST: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_const(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_VOLATILE: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_volatile(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_RESTRICT: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_restrict(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_ARRAY: {
		btf_array_t	*arr = (btf_array_t *)(type + 1);
		ctf_arinfo_t	arp;

		arp.ctr_contents = dt_btf_add_to_ctf(dtp, btf, ctf, arr->type);
		if (arp.ctr_contents == CTF_ERR)
			return CTF_ERR;
		arp.ctr_index = dt_btf_add_to_ctf(dtp, btf, ctf, arr->index_type);
		if (arp.ctr_contents == CTF_ERR)
			return CTF_ERR;
		arp.ctr_nelems = arr->nelems;

		ctfid = ctf_add_array(ctf, CTF_ADD_ROOT, &arp);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_TYPEDEF: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_typedef(ctf, CTF_ADD_ROOT, name, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_STRUCT:
	case BTF_KIND_UNION: {
		int		i;
		btf_member_t	*item = (btf_member_t *)(type + 1);

		if (kind == BTF_KIND_STRUCT)
			ctfid = ctf_add_struct_sized(ctf, CTF_ADD_ROOT, name,
						     type->size);
		else
			ctfid = ctf_add_union_sized(ctf, CTF_ADD_ROOT, name,
						    type->size);

		btf->ctfids[type_id] = ctfid;
		if (ctfid == CTF_ERR)
			return dt_ctf_error(dtp, ctf);

		for (i = 0; i < vlen; i++, item++) {
			int		err;
			ctf_id_t	mbid;
			const char	*mname;

			mbid = dt_btf_add_to_ctf(dtp, btf, ctf, item->type);
			if (mbid == CTF_ERR)
				return CTF_ERR;
			mname = dt_btf_get_string(dtp, btf, item->name_off);

			if (BTF_INFO_KFLAG(type->info)) {
				enc.cte_format = 0;
				enc.cte_offset = 0;
				enc.cte_bits = BTF_MEMBER_BITFIELD_SIZE(item->offset);
				item->offset = BTF_MEMBER_BIT_OFFSET(item->offset);

				if (enc.cte_bits > 0) {
					int	kind;

					/* Resolve member type for CTF. */
					mbid = ctf_type_resolve(ctf, mbid);
					kind = ctf_type_kind(ctf, mbid);

					if (kind != CTF_K_INTEGER &&
					    kind != CTF_K_FLOAT &&
					    kind != CTF_K_ENUM)
						return dt_ctf_set_errno(
							dtp, ECTF_NOTINTFP);

					mbid = ctf_add_slice(ctf,
							     CTF_ADD_NONROOT,
							     mbid, &enc);
					if (mbid == CTF_ERR)
						return dt_ctf_error(dtp, ctf);

					/* Fall-through */
				}

				/* Fall-through - add as regular member. */
			}

			err = ctf_add_member_offset(ctf, ctfid, mname, mbid,
						    item->offset);
			if (err == CTF_ERR)
				return dt_ctf_error(dtp, ctf);
		}

		return ctfid;
	}
	case BTF_KIND_FUNC_PROTO: {
		int		i;
		ctf_id_t	argv[vlen];
		btf_param_t	*item = (btf_param_t *)(type + 1);
		ctf_funcinfo_t	ctc;

		ctc.ctc_flags = 0;
		ctc.ctc_argc = vlen;
		ctc.ctc_return = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctc.ctc_return == CTF_ERR)
			return CTF_ERR;

		for (i = 0; i < vlen; i++) {
			argv[i] = dt_btf_add_to_ctf(dtp, btf, ctf, item->type);
			if (argv[i] == CTF_ERR)
				return CTF_ERR;
		}

		ctfid = ctf_add_function(ctf, CTF_ADD_ROOT, &ctc, argv);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_FWD: {
		uint32_t	kind;

		kind = BTF_INFO_KFLAG(type->info) ? CTF_K_UNION : CTF_K_STRUCT;
		ctfid = ctf_add_forward(ctf, CTF_ADD_ROOT, name, kind);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_VAR:
	case BTF_KIND_DATASEC:
	case BTF_KIND_DECL_TAG:
	case BTF_KIND_TYPE_TAG:
	case BTF_KIND_ENUM64:
	case BTF_KIND_FUNC:
		return btf->ctfids[0];		/* Ignored for CTF */
	default:
		return dt_ctf_error(dtp, ctf);
	}

	assert(0);
}

ctf_dict_t *
dt_btf_to_ctf(dtrace_hdl_t *dtp, dt_module_t *dmp, dt_btf_t *btf)
{
	int		i, base = 0;
	ctf_dict_t	*ctf;
	ctf_encoding_t	enc = { CTF_INT_SIGNED, 0, 0 };

	ctf = ctf_create(&dtp->dt_ctferr);
	if (ctf == NULL)
		return NULL;

	ctf_setmodel(ctf, dtp->dt_conf.dtc_ctfmodel);

	/*
	 * All modules import 'shared_ctf'.  The 'vmlinux' module is handled as
	 * a special case because it simply imports 'shared_ctf'.
	 */
	if (dmp) {
		int	err;

		err = ctf_import(ctf, dtp->dt_shared_ctf);
		if (err == CTF_ERR)
			return NULL;

		ctf_setspecific(ctf, dmp);

		if (strcmp(dmp->dm_name, "vmlinux") == 0)
			return ctf;
	}

	btf->ctfids = dt_calloc(dtp, btf->type_cnt, sizeof(ctf_id_t));
	for (i = 1; i < btf->type_cnt; i++)
		btf->ctfids[i] = CTF_ERR;

	/*
	 * If we are constructing 'shared_ctf' (no module), we need to add an
	 * entry for the implicit 'void' BTF type (id 0).
	 */
	if (!dmp) {
		btf->ctfids[0] = ctf_add_integer(ctf, CTF_ADD_ROOT, "void",
						 &enc);
		if (btf->ctfids[0] == CTF_ERR)
			dt_dprintf("Could not create 'void' CTF entry.\n");
	}

	/*
	 * Any module other than 'vmlinux' inherits the types from 'vmlinux'.
	 * The shared types are 1 through (base = dtp->dt_btf->type_cnt - 1).
	 * A module's types are base through (base + btf->type_cnt - 1), but
	 * the types are stored in the BTF types array with indexes 1 through
	 * (btf->type_cnt - 1).
	 */
	if (dtp->dt_btf)
		base = dtp->dt_btf->type_cnt - 1;

	for (i = 1; i < btf->type_cnt; i++) {
		int	type_id = i + base;

		if (dt_btf_add_to_ctf(dtp, btf, ctf, type_id) == CTF_ERR) {
			dt_dprintf("BTF-to-CTF error: %s\n",
				   ctf_errmsg(dtp->dt_ctferr));
			break;
		}
	}

	if (ctf_update(ctf) == CTF_ERR)
		return NULL;

	return ctf;
}

ctf_dict_t *
dt_btf_load_module(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	char		fn[PATH_MAX + 1];
	dt_btf_t	*btf;

	snprintf(fn, sizeof(fn), "/sys/kernel/btf/%s", dmp->dm_name);
	btf = dt_btf_load_file(dtp, fn);
	if (btf == NULL)
		return NULL;

	if (strcmp(dmp->dm_name, "vmlinux") == 0) {
		dtp->dt_shared_ctf = dt_btf_to_ctf(dtp, NULL, btf);
		dtp->dt_btf = btf;

		dt_dprintf("Generated shared CTF from BTF (%d types).\n",
			   btf->type_cnt);

		return dtp->dt_shared_ctf;
	}

	dt_dprintf("Generated %s CTF from BTF (%d types).\n", dmp->dm_name,
		   btf->type_cnt);

	return dt_btf_to_ctf(dtp, dmp, btf);
}

const char *
dt_btf_get_string(dtrace_hdl_t *dtp, dt_btf_t *btf, uint32_t off)
{
	if (dtp->dt_btf == NULL)
		goto ok;

	/* Check if the offset is within the base BTF string area. */
	if (btf == dtp->dt_btf || off < dtp->dt_btf->hdr->str_len)
		return dtp->dt_btf->sdata + off;

	off -= dtp->dt_btf->hdr->str_len;
ok:
	if (off < btf->hdr->str_len)
		return btf->sdata + off;

	return dt_btf_set_load_errno(dtp, EINVAL);
}

int32_t
dt_btf_lookup_name_kind(dtrace_hdl_t *dtp, dt_btf_t *btf, const char *name,
			uint32_t kind)
{
	int32_t	i, base = 0;

	if (kind == BTF_KIND_UNKN)
		return 0;
	if (strcmp(name, "void") == 0)
		return 0;

	/*
	 * Ensure the shared BTF is loaded, and if no BTF is given, use the
	 * shared one.
	 */
	 if (!dtp->dt_btf) {
		  dt_btf_load_module(dtp, dtp->dt_exec);

		  if (!btf)
			   btf = dtp->dt_btf;
	 }

	 if (!btf)
		  return -ENOENT;

	/*
	 * Any module other than 'vmlinux' inherits the types from 'vmlinux'.
	 * The shared types are 1 through (base = dtp->dt_btf->type_cnt - 1).
	 * A module's types are base through (base + btf->type_cnt - 1), but
	 * the types are stored in the BTF types array with indexes 1 through
	 * (btf->type_cnt - 1).
	 */
	if (btf != dtp->dt_btf)
		base = dtp->dt_btf->type_cnt - 1;


	for (i = 1; i < btf->type_cnt; i++) {
		const btf_type_t	*type = btf->types[i];
		const char		*str;

		if (BTF_INFO_KIND(type->info) != kind)
			continue;

		str = dt_btf_get_string(dtp, btf, type->name_off);
		if (str && strcmp(name, str) == 0)
			return i + base;
	}

	if (base > 0)
		return dt_btf_lookup_name_kind(dtp, dtp->dt_btf, name, kind);

	return -ENOENT;
}
