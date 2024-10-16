/*
 * Oracle Linux DTrace; DOF parser.
 * Copyright (c) 2010, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/compiler.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dof_parser.h"

#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a) - 1)) == 0)

size_t			dof_maxsize = 256 * 1024 * 1024;

typedef struct dtrace_helper_probedesc {
	char *dthpb_prov;
	char *dthpb_mod;
	char *dthpb_func;
	char *dthpb_name;
	uint64_t dthpb_base;
	uint32_t *dthpb_offs;
	uint32_t *dthpb_enoffs;
	uint32_t dthpb_noffs;
	uint32_t dthpb_nenoffs;
	uint8_t *dthpb_args;
	uint8_t dthpb_xargc;
	uint8_t dthpb_nargc;
	char *dthpb_xtypes;
	char *dthpb_ntypes;
} dtrace_helper_probedesc_t;

static void dt_dbg_dof(const char *fmt, ...)
{
#ifdef DOF_DEBUG
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
#endif
}

_dt_printflike_(3, 4)
static void dof_error(int out, int err_no, const char *fmt, ...)
{
	dof_parsed_t *parsed;
	size_t sz;
	char *msg;
	va_list ap;

	/*
	 * Not much we can do on OOM of errors other than abort, forcing a
	 * parser restart, which hopefully will have enough memory to report the
	 * error properly.
	 */
	va_start(ap, fmt);
	if (vasprintf(&msg, fmt, ap) < 0)
		abort();
	va_end(ap);

	sz = offsetof(dof_parsed_t, err.err) + strlen(msg) + 1;
	parsed = malloc(sz);

	if (!parsed)
		abort();

	memset(parsed, 0, sz);
	parsed->size = sz;
	parsed->type = DIT_ERR;
	parsed->err.err_no = err_no;
	strcpy(parsed->err.err, msg);

	dof_parser_write_one(out, parsed, parsed->size);
	free(parsed);
	free(msg);
}

dof_helper_t *
dof_copyin_helper(int in, int out, int *ok)
{
	dof_helper_t *dh;
	size_t i;

	/*
	 * First get the header, which gives the size of everything else.
	 */
	dh = malloc(sizeof(dof_helper_t));
	if (!dh)
		abort();

	memset(dh, 0, sizeof(dof_helper_t));

	for (i = 0; i < sizeof(dof_helper_t);) {
		size_t ret;

		ret = read(in, ((char *) dh) + i, sizeof(dof_helper_t) - i);

		if (ret < 0) {
			switch (errno) {
			case EINTR:
				continue;
			default:
				goto err;
			}
		}

		/*
		 * EOF: parsing done, process shutting down or message
		 * truncated.  Fail, in any case.
		 */
		if (ret == 0)
			goto err;

		i += ret;
	}

	*ok = 1;
	return dh;

err:
	*ok = 0;
	free(dh);
	return NULL;
}

dof_hdr_t *
dof_copyin_dof(int in, int out, int *ok)
{
	dof_hdr_t *dof;
	size_t i, sz;

	*ok = 1;

	/*
	 * First get the header, which gives the size of everything else.
	 */
	dof = malloc(sizeof(dof_hdr_t));
	if (!dof)
		abort();

	memset(dof, 0, sizeof(dof_hdr_t));

	for (i = 0, sz = sizeof(dof_hdr_t); i < sz;) {
		size_t ret;

		ret = read(in, ((char *) dof) + i, sz - i);

		if (ret < 0) {
			switch (errno) {
			case EINTR:
				continue;
			default:
				goto err;
			}
		}

		/*
		 * EOF: parsing done, process shutting down or message
		 * truncated.  Fail, in any case.
		 */
		if (ret == 0)
			goto err;

		/* Allocate more room if needed for the reply.  */
		if (i < sizeof(dof_hdr_t) &&
		    i + ret >= sizeof(dof_hdr_t)) {
			dof_hdr_t *new_dof;

			if (dof->dofh_loadsz >= dof_maxsize) {
				dof_error(out, E2BIG, "load size %zi exceeds maximum %zi",
					  dof->dofh_loadsz, dof_maxsize);
				return NULL;
			}

			if (dof->dofh_loadsz < sizeof(dof_hdr_t)) {
				dof_error(out, EINVAL, "invalid load size %zi, "
					  "smaller than header size %zi", dof->dofh_loadsz,
					  sizeof(dof_hdr_t));
				return NULL;
			}

			new_dof = realloc(dof, dof->dofh_loadsz);
			if (!new_dof)
				abort();

			memset(((char *)new_dof) + i + ret, 0, new_dof->dofh_loadsz - (i + ret));
			dof = new_dof;
			sz = dof->dofh_loadsz;
		}

		i += ret;
	}

	return dof;

err:
	*ok = 0;
	free(dof);
	return NULL;
}

static void dof_destroy(dof_helper_t *dhp, dof_hdr_t *dof)
{
	free(dhp);
	free(dof);
}

/*
 * Return the dof_sec_t pointer corresponding to a given section index.  If the
 * index is not valid, dof_error() is called and NULL is returned.  If a type
 * other than DOF_SECT_NONE is specified, the header is checked against this
 * type and NULL is returned if the types do not match.
 */
static dof_sec_t *dof_sect(int out, dof_hdr_t *dof,
			   uint32_t sectype, dof_secidx_t i)
{
	dof_sec_t *sec;

	sec = (dof_sec_t *)(uintptr_t) ((uintptr_t)dof +
					dof->dofh_secoff +
					i * dof->dofh_secsize);

	if (i >= dof->dofh_secnum) {
		dof_error(out, EINVAL, "referenced section index %u is "
			  "invalid, above %u", i, dof->dofh_secnum);
		return NULL;
	}

	if (!(sec->dofs_flags & DOF_SECF_LOAD)) {
		dof_error(out, EINVAL, "referenced section %u is not loadable", i);
		return NULL;
	}

	if (sectype != DOF_SECT_NONE && sectype != sec->dofs_type) {
		dof_error(out, EINVAL, "referenced section %u is the wrong type, "
			  "%u, not %u", i, sec->dofs_type, sectype);
		return NULL;
	}

	return sec;
}

/*
 * Apply the relocations from the specified 'sec' (a DOF_SECT_URELHDR) to the
 * specified DOF.  At present, this amounts to simply adding 'ubase' to the
 * site of any user SETX relocations to account for load object base address.
 * In the future, if we need other relocations, this function can be extended.
 */
static int
dof_relocate(int out, dof_hdr_t *dof, dof_sec_t *sec, uint64_t ubase)
{
	uintptr_t	daddr = (uintptr_t)dof;
	dof_relohdr_t	*dofr;
	dof_sec_t	*ss, *rs, *ts;
	dof_relodesc_t	*r;
	uint_t		i, n;

	dofr = (dof_relohdr_t *)(uintptr_t) (daddr + sec->dofs_offset);

	if (sec->dofs_size < sizeof(dof_relohdr_t) ||
	    sec->dofs_align != sizeof(dof_secidx_t)) {
		dof_error(out, EINVAL, "invalid relocation header: "
			  "size %zi (expected %zi); alignment %u (expected %zi)",
			  sec->dofs_size, sizeof(dof_relohdr_t),
			  sec->dofs_align, sizeof(dof_secidx_t));
		return -1;
	}

	ss = dof_sect(out, dof, DOF_SECT_STRTAB, dofr->dofr_strtab);
	rs = dof_sect(out, dof, DOF_SECT_RELTAB, dofr->dofr_relsec);
	ts = dof_sect(out, dof, DOF_SECT_NONE, dofr->dofr_tgtsec);

	if (ss == NULL || rs == NULL || ts == NULL)
		return -1; /* dof_error() has been called already */

	if (rs->dofs_entsize < sizeof(dof_relodesc_t) ||
	    rs->dofs_align != sizeof(uint64_t)) {
		dof_error(out, EINVAL, "invalid relocation section: entsize %i "
			  "(expected %zi); alignment %u (expected %zi)",
			  rs->dofs_entsize, sizeof(dof_relodesc_t),
			  rs->dofs_align, sizeof(uint64_t));
		return -1;
	}

	r = (dof_relodesc_t *)(uintptr_t)(daddr + rs->dofs_offset);
	n = rs->dofs_size / rs->dofs_entsize;

	for (i = 0; i < n; i++) {
		uintptr_t taddr = daddr + ts->dofs_offset + r->dofr_offset;

		switch (r->dofr_type) {
		case DOF_RELO_NONE:
			break;
		case DOF_RELO_SETX:
			if (r->dofr_offset >= ts->dofs_size ||
			    r->dofr_offset + sizeof(uint64_t) >
				ts->dofs_size) {
				dof_error(out, EINVAL, "bad relocation offset: "
					  "offset %zi, section size %zi)",
					  r->dofr_offset, ts->dofs_size);
				return -1;
			}

			if (!IS_ALIGNED(taddr, sizeof(uint64_t))) {
				dof_error(out, EINVAL, "misaligned setx relo");
				return -1;
			}

			/*
			 * This is a bit ugly but it is necessary because some
			 * linkers retain the relocation records for the
			 * .SUNW_dof section in shared libraries.  In that case,
			 * the runtime loader already performed the relocation,
			 * so we do not have to do anything here.
			 */
			if (*(uint64_t *)taddr > ubase)
				dt_dbg_dof("      Relocation by runtime " \
					   "loader: 0x%llx (base 0x%llx)\n",
					   *(uint64_t *)taddr, ubase);
			else {
				dt_dbg_dof("      Relocate 0x%llx + 0x%llx " \
					   "= 0x%llx\n",
					   *(uint64_t *)taddr, ubase,
					   *(uint64_t *)taddr + ubase);

				*(uint64_t *)taddr += ubase;
			}

			break;
		default:
			dof_error(out, EINVAL, "invalid relocation type %i",
				r->dofr_type);
			return -1;
		}

		r = (dof_relodesc_t *)((uintptr_t)r + rs->dofs_entsize);
	}

	return 0;
}

/*
 * The dof_hdr_t passed to dof_slurp() should be a partially validated
 * header:  it should be at the front of a memory region that is at least
 * sizeof(dof_hdr_t) in size -- and then at least dof_hdr.dofh_loadsz in
 * size.  It need not be validated in any other way.
 */
static int
dof_slurp(int out, dof_hdr_t *dof, uint64_t ubase)
{
	uint64_t	len = dof->dofh_loadsz, seclen;
	uintptr_t	daddr = (uintptr_t)dof;
	uint_t		i;

	if (_dt_unlikely_(dof->dofh_loadsz < sizeof(dof_hdr_t))) {
		dof_error(out, EINVAL, "load size %zi smaller than header %zi",
			  dof->dofh_loadsz, sizeof(dof_hdr_t));
		return -1;
	}

	dt_dbg_dof("  DOF 0x%p Slurping...\n", dof);

	dt_dbg_dof("    DOF 0x%p Validating...\n", dof);

	/*
	 * Check the DOF header identification bytes.  In addition to checking
	 * valid settings, we also verify that unused bits/bytes are zeroed so
	 * we can use them later without fear of regressing existing binaries.
	 */
	if (memcmp(&dof->dofh_ident[DOF_ID_MAG0], DOF_MAG_STRING,
		   DOF_MAG_STRLEN) != 0) {
		dof_error(out, EINVAL, "DOF magic string mismatch: %c%c%c%c "
			  "versus %c%c%c%c\n", dof->dofh_ident[DOF_ID_MAG0],
			  dof->dofh_ident[DOF_ID_MAG1],
			  dof->dofh_ident[DOF_ID_MAG2],
			  dof->dofh_ident[DOF_ID_MAG3],
			  DOF_MAG_STRING[0],
			  DOF_MAG_STRING[1],
			  DOF_MAG_STRING[2],
			  DOF_MAG_STRING[3]);
		return -1;
	}

	if (dof->dofh_ident[DOF_ID_MODEL] != DOF_MODEL_ILP32 &&
	    dof->dofh_ident[DOF_ID_MODEL] != DOF_MODEL_LP64) {
		dof_error(out, EINVAL, "DOF has invalid data model: %i",
			  dof->dofh_ident[DOF_ID_MODEL]);
		return -1;
	}

	if (dof->dofh_ident[DOF_ID_ENCODING] != DOF_ENCODE_NATIVE) {
		dof_error(out, EINVAL, "DOF encoding mismatch: %i, expected %i",
			  dof->dofh_ident[DOF_ID_ENCODING], DOF_ENCODE_NATIVE);
		return -1;
	}

	if (dof->dofh_ident[DOF_ID_VERSION] != DOF_VERSION_1 &&
	    dof->dofh_ident[DOF_ID_VERSION] != DOF_VERSION_2 &&
	    dof->dofh_ident[DOF_ID_VERSION] != DOF_VERSION_3) {
		dof_error(out, EINVAL, "DOF version mismatch: %i",
			  dof->dofh_ident[DOF_ID_VERSION]);
		return -1;
	}

	if (dof->dofh_ident[DOF_ID_DIFVERS] != DIF_VERSION_2) {
		dof_error(out, EINVAL, "DOF uses unsupported instruction set %i",
			dof->dofh_ident[DOF_ID_DIFVERS]);
		return -1;
	}

	if (dof->dofh_ident[DOF_ID_DIFIREG] > DIF_DIR_NREGS) {
		dof_error(out, EINVAL, "DOF uses too many integer registers: %i > %i",
			  dof->dofh_ident[DOF_ID_DIFIREG], DIF_DIR_NREGS);
		return -1;
	}

	if (dof->dofh_ident[DOF_ID_DIFTREG] > DIF_DTR_NREGS) {
		dof_error(out, EINVAL, "DOF uses too many tuple registers: %i > %i",
			  dof->dofh_ident[DOF_ID_DIFTREG], DIF_DTR_NREGS);
		return -1;
	}

	for (i = DOF_ID_PAD; i < DOF_ID_SIZE; i++) {
		if (dof->dofh_ident[i] != 0) {
			dof_error(out, EINVAL, "DOF has invalid ident byte set: %i = %i",
				  i, dof->dofh_ident[i]);
			return -1;
		}
	}

	if (dof->dofh_flags & ~DOF_FL_VALID) {
		dof_error(out, EINVAL, "DOF has invalid flag bits set: %xi", dof->dofh_flags);
		return -1;
	}

	if (dof->dofh_secsize == 0) {
		dof_error(out, EINVAL, "zero section header size");
		return -1;
	}

	/*
	 * Check that the section headers don't exceed the amount of DOF
	 * data.  Note that we cast the section size and number of sections
	 * to uint64_t's to prevent possible overflow in the multiplication.
	 */
	seclen = (uint64_t)dof->dofh_secnum * (uint64_t)dof->dofh_secsize;

	if (dof->dofh_secoff > len || seclen > len ||
	    dof->dofh_secoff + seclen > len) {
		dof_error(out, EINVAL, "truncated section headers: %zi, %zi, %zi",
			  dof->dofh_secoff, len, seclen);
		return -1;
	}

	if (!IS_ALIGNED(dof->dofh_secoff, sizeof(uint64_t))) {
		dof_error(out, EINVAL, "misaligned section headers");
		return -1;
	}

	if (!IS_ALIGNED(dof->dofh_secsize, sizeof(uint64_t))) {
		dof_error(out, EINVAL, "misaligned section size");
		return -1;
	}

	/*
	 * Take an initial pass through the section headers to be sure that
	 * the headers don't have stray offsets. 
	 */
	dt_dbg_dof("    DOF 0x%p Checking section offsets...\n", dof);

	for (i = 0; i < dof->dofh_secnum; i++) {
		dof_sec_t *sec;

		sec = (dof_sec_t *)(daddr + (uintptr_t)dof->dofh_secoff +
				    i * dof->dofh_secsize);

		if (DOF_SEC_ISLOADABLE(sec->dofs_type) &&
		    !(sec->dofs_flags & DOF_SECF_LOAD)) {
			dof_error(out, EINVAL, "loadable section %i with load flag unset",
				i);
			return -1;
		}

		/*
		 * Just ignore non-loadable sections.
		 */
		if (!(sec->dofs_flags & DOF_SECF_LOAD))
			continue;

		if (sec->dofs_align & (sec->dofs_align - 1)) {
			dof_error(out, EINVAL, "bad section %i alignment %x", i,
				sec->dofs_align);
			return -1;
		}

		if (sec->dofs_offset & (sec->dofs_align - 1)) {
			dof_error(out, EINVAL, "misaligned section %i: %lx, "
				  "stated alignment %xi", i, sec->dofs_offset,
				  sec->dofs_align);
			return -1;
		}

		if (sec->dofs_offset > len || sec->dofs_size > len ||
		    sec->dofs_offset + sec->dofs_size > len) {
			dof_error(out, EINVAL, "corrupt section %i header: "
				  "offset %lx, size %lx, len %lx", i,
				  sec->dofs_offset, sec->dofs_size, len);
			return -1;
		}

		if (sec->dofs_type == DOF_SECT_STRTAB && *((char *)daddr +
		    sec->dofs_offset + sec->dofs_size - 1) != '\0') {
			dof_error(out, EINVAL, "section %i: non-0-terminated "
				  "string table", i);
			return -1;
		}
	}

	/*
	 * Take a second pass through the sections and locate and perform any
	 * relocations that are present.  We do this after the first pass to
	 * be sure that all sections have had their headers validated.
	 */
	dt_dbg_dof("    DOF 0x%p Performing relocations...\n", dof);

	for (i = 0; i < dof->dofh_secnum; i++) {
		dof_sec_t *sec;

		sec = (dof_sec_t *)(daddr + (uintptr_t)dof->dofh_secoff +
				    i * dof->dofh_secsize);

		/*
		 * Skip sections that are not loadable.
		 */
		if (!(sec->dofs_flags & DOF_SECF_LOAD))
			continue;

		switch (sec->dofs_type) {
		case DOF_SECT_URELHDR:
			if (dof_relocate(out, dof, sec, ubase) != 0)
				return -1;
			break;
		}
	}

	dt_dbg_dof("  DOF 0x%p Done slurping\n", dof);

	return 0;
}

static int
validate_provider(int out, dof_hdr_t *dof, dof_sec_t *sec)
{
	uintptr_t	daddr = (uintptr_t)dof;
	dof_sec_t	*str_sec, *prb_sec, *arg_sec, *off_sec, *enoff_sec;
	dof_provider_t	*prov;
	dof_probe_t	*prb;
	uint8_t		*arg;
	char		*strtab, *typestr;
	dof_stridx_t	typeidx;
	size_t		typesz;
	uint_t		nprobes, j, k;

	if (_dt_unlikely_(sec->dofs_type != DOF_SECT_PROVIDER)) {
		dof_error(out, EINVAL, "DOF is not provider DOF: %i", sec->dofs_type);
		return -1;
	}

	if (sec->dofs_offset & (sizeof(uint_t) - 1)) {
		dof_error(out, EINVAL, "misaligned section offset: %lx",
			sec->dofs_offset);
		return -1;
	}

	/*
	 * The section needs to be large enough to contain the DOF provider
	 * structure appropriate for the given version.
	 */
	if (sec->dofs_size <
	    ((dof->dofh_ident[DOF_ID_VERSION] == DOF_VERSION_1)
			? offsetof(dof_provider_t, dofpv_prenoffs)
			: sizeof(dof_provider_t))) {
		dof_error(out, EINVAL, "provider section too small: %lx",
			sec->dofs_size);
		return -1;
	}

	prov = (dof_provider_t *)(uintptr_t)(daddr + sec->dofs_offset);
	str_sec = dof_sect(out, dof, DOF_SECT_STRTAB, prov->dofpv_strtab);
	prb_sec = dof_sect(out, dof, DOF_SECT_PROBES, prov->dofpv_probes);
	arg_sec = dof_sect(out, dof, DOF_SECT_PRARGS, prov->dofpv_prargs);
	off_sec = dof_sect(out, dof, DOF_SECT_PROFFS, prov->dofpv_proffs);

	if (str_sec == NULL || prb_sec == NULL ||
	    arg_sec == NULL || off_sec == NULL)
		return -1;

	enoff_sec = NULL;

	if (dof->dofh_ident[DOF_ID_VERSION] != DOF_VERSION_1 &&
	    prov->dofpv_prenoffs != DOF_SECT_NONE) {
		enoff_sec = dof_sect(out, dof, DOF_SECT_PRENOFFS,
				     prov->dofpv_prenoffs);

		if (enoff_sec == NULL)
			return -1;
	}

	strtab = (char *)(uintptr_t)(daddr + str_sec->dofs_offset);

	if (prov->dofpv_name >= str_sec->dofs_size) {
		dof_error(out, EINVAL, "invalid provider name offset: %u > %zi",
			  prov->dofpv_name, str_sec->dofs_size);
		return -1;
	}

	if (strlen(strtab + prov->dofpv_name) >= DTRACE_PROVNAMELEN) {
		dof_error(out, EINVAL, "provider name too long: %s",
			  strtab + prov->dofpv_name);
		return -1;
	}

	if (prb_sec->dofs_entsize == 0 ||
	    prb_sec->dofs_entsize > prb_sec->dofs_size) {
		dof_error(out, EINVAL, "invalid entry size %x, max %lx",
			  prb_sec->dofs_entsize, prb_sec->dofs_size);
		return -1;
	}

	if (prb_sec->dofs_entsize & (sizeof(uintptr_t) - 1)) {
		dof_error(out, EINVAL, "misaligned entry size %x",
			  prb_sec->dofs_entsize);
		return -1;
	}

	if (off_sec->dofs_entsize != sizeof(uint32_t)) {
		dof_error(out, EINVAL, "invalid entry size %x",
			  off_sec->dofs_entsize);
		return -1;
	}

	if (off_sec->dofs_offset & (sizeof(uint32_t) - 1)) {
		dof_error(out, EINVAL, "misaligned section offset %lx",
			  off_sec->dofs_offset);
		return -1;
	}

	if (arg_sec->dofs_entsize != sizeof(uint8_t)) {
		dof_error(out, EINVAL, "invalid entry size %x",
			  arg_sec->dofs_entsize);
		return -1;
	}

	arg = (uint8_t *)(uintptr_t)(daddr + arg_sec->dofs_offset);
	nprobes = prb_sec->dofs_size / prb_sec->dofs_entsize;

	dt_dbg_dof("    DOF 0x%p %s::: with %d probes\n",
		   dof, strtab + prov->dofpv_name, nprobes);

	/*
	 * Take a pass through the probes to check for errors.
	 */
	for (j = 0; j < nprobes; j++) {
		prb = (dof_probe_t *)(uintptr_t)
			(daddr + prb_sec->dofs_offset +
			 j * prb_sec->dofs_entsize);

		if (prb->dofpr_func >= str_sec->dofs_size) {
			dof_error(out, EINVAL, "invalid function name: "
				  "strtab offset %x, max %lx", prb->dofpr_func,
				  str_sec->dofs_size);
			return -1;
		}

		if (strlen(strtab + prb->dofpr_func) >= DTRACE_FUNCNAMELEN) {
			dof_error(out, EINVAL, "function name %s too long",
				  strtab + prb->dofpr_func);
			return -1;
		}

		if (prb->dofpr_name >= str_sec->dofs_size) {
			dof_error(out, EINVAL, "invalid probe name: "
				  "strtab offset %x, max %lx", prb->dofpr_name,
				str_sec->dofs_size);
			return -1;
		}

		if (strlen(strtab + prb->dofpr_name) >= DTRACE_NAMELEN) {
			dof_error(out, EINVAL, "probe name %s too long",
				strtab + prb->dofpr_name);
			return -1;
		}

		/*
		 * The offset count must not wrap the index, and the offsets
		 * must also not overflow the section's data.
		 */
		if (prb->dofpr_offidx + prb->dofpr_noffs < prb->dofpr_offidx ||
		    (prb->dofpr_offidx + prb->dofpr_noffs) *
		    off_sec->dofs_entsize > off_sec->dofs_size) {
			dof_error(out, EINVAL, "invalid probe offset %x "
				  "(offset count %x, section entsize %x, size %lx)",
				  prb->dofpr_offidx, prb->dofpr_noffs,
				  off_sec->dofs_entsize, off_sec->dofs_size);
			return -1;
		}

		if (dof->dofh_ident[DOF_ID_VERSION] != DOF_VERSION_1) {
			/*
			 * If there's no is-enabled offset section, make sure
			 * there aren't any is-enabled offsets. Otherwise
			 * perform the same checks as for probe offsets
			 * (immediately above).
			 */
			if (enoff_sec == NULL) {
				if (prb->dofpr_enoffidx != 0 ||
				    prb->dofpr_nenoffs != 0) {
					dof_error(out, EINVAL,
						  "is-enabled offsets with null section");
					return -1;
				}
			} else if (prb->dofpr_enoffidx + prb->dofpr_nenoffs <
				   prb->dofpr_enoffidx ||
				   (prb->dofpr_enoffidx + prb->dofpr_nenoffs) *
				   enoff_sec->dofs_entsize >
				   enoff_sec->dofs_size) {
				dof_error(out, EINVAL, "invalid is-enabled offset %x "
					  "(offset count %x, section entsize %x, size %lx)",
					  prb->dofpr_enoffidx, prb->dofpr_nenoffs,
					  enoff_sec->dofs_entsize, enoff_sec->dofs_size);
				return -1;
			}

			if (prb->dofpr_noffs + prb->dofpr_nenoffs == 0) {
				dof_error(out, EINVAL, "zero probe and is-enabled offsets");
				return -1;
			}
		} else if (prb->dofpr_noffs == 0) {
			dof_error(out, EINVAL, "zero probe offsets");
			return -1;
		}

		if (prb->dofpr_argidx + prb->dofpr_xargc < prb->dofpr_argidx ||
		    (prb->dofpr_argidx + prb->dofpr_xargc) *
		    arg_sec->dofs_entsize > arg_sec->dofs_size) {
			dof_error(out, EINVAL, "invalid args, idx %x "
				  "(offset count %x, section entsize %x, size %lx)",
				  prb->dofpr_argidx, prb->dofpr_xargc,
				  arg_sec->dofs_entsize, arg_sec->dofs_size);
			return -1;
		}

		typeidx = prb->dofpr_nargv;
		typestr = strtab + prb->dofpr_nargv;
		for (k = 0; k < prb->dofpr_nargc; k++) {
			if (typeidx >= str_sec->dofs_size) {
				dof_error(out, EINVAL, "bad native argument type "
					  "for arg %i: %x", k, typeidx);
				return -1;
			}

			typesz = strlen(typestr) + 1;
			if (typesz > DTRACE_ARGTYPELEN) {
				dof_error(out, EINVAL, "native argument type for arg %i "
					  "too long: %s", k, typestr);
				return -1;
			}

			typeidx += typesz;
			typestr += typesz;
		}

		typeidx = prb->dofpr_xargv;
		typestr = strtab + prb->dofpr_xargv;
		for (k = 0; k < prb->dofpr_xargc; k++) {
			if (arg[prb->dofpr_argidx + k] > prb->dofpr_nargc) {
				dof_error(out, EINVAL, "bad native argument index "
					  "for arg %i: %i (max %i)", k,
					  arg[prb->dofpr_argidx + k],
					  prb->dofpr_nargc);
				return -1;
			}

			if (typeidx >= str_sec->dofs_size) {
				dof_error(out, EINVAL, "bad translated argument type "
					  "for arg %i: %x", k, typeidx);
				return -1;
			}

			typesz = strlen(typestr) + 1;
			if (typesz > DTRACE_ARGTYPELEN) {
				dof_error(out, EINVAL, "translated argument type for arg %i "
					  "too long: %s", k, typestr);
				return -1;
			}

			typeidx += typesz;
			typestr += typesz;
		}

		dt_dbg_dof("      Probe %d %s:%s:%s:%s with %d offsets, "
			   "%d is-enabled offsets, %i args, %i nargs, argidx %i\n", j,
			   strtab + prov->dofpv_name, "",
			   strtab + prb->dofpr_func, strtab + prb->dofpr_name,
			   prb->dofpr_noffs, prb->dofpr_nenoffs,
			   prb->dofpr_xargc, prb->dofpr_nargc, prb->dofpr_argidx);
	}

	return 0;
}

static void
emit_tp(int out, uint64_t base, uint64_t offs, int is_enabled)
{
	dof_parsed_t tp;

	memset(&tp, 0, sizeof(tp));

	tp.size = sizeof(dof_parsed_t);
	tp.type = DIT_TRACEPOINT;
	tp.tracepoint.addr = base + offs;
	tp.tracepoint.is_enabled = is_enabled;
	dof_parser_write_one(out, &tp, tp.size);

	dt_dbg_dof("        Tracepoint at 0x%lx (0x%llx + 0x%x)%s\n",
		   base + offs, base, offs, is_enabled ? " (is_enabled)" : "");
}

static int
uint32_cmp(const void *ap, const void *bp)
{
	return (*(const uint32_t *)ap - *(const uint32_t *)bp);
}

static int
validate_probe(int out, dtrace_helper_probedesc_t *dhpb)
{
	int	i;

	/*
	 * The offsets must be unique.
	 */
	qsort(dhpb->dthpb_offs, dhpb->dthpb_noffs, sizeof(uint32_t),
	     uint32_cmp);
	for (i = 1; i < dhpb->dthpb_noffs; i++) {
		if (dhpb->dthpb_base + dhpb->dthpb_offs[i] <=
		    dhpb->dthpb_base + dhpb->dthpb_offs[i - 1]) {
			dof_error(out, EINVAL, "non-unique USDT offsets at %i: %li <= %li",
				  i, dhpb->dthpb_base + dhpb->dthpb_offs[i],
				  dhpb->dthpb_base + dhpb->dthpb_offs[i - 1]);
			return -1;
		}
	}

	qsort(dhpb->dthpb_enoffs, dhpb->dthpb_nenoffs, sizeof(uint32_t),
	     uint32_cmp);
	for (i = 1; i < dhpb->dthpb_nenoffs; i++) {
		if (dhpb->dthpb_base + dhpb->dthpb_enoffs[i] <=
		    dhpb->dthpb_base + dhpb->dthpb_enoffs[i - 1]) {
			dof_error(out, EINVAL, "non-unique is-enabled USDT offsets "
				  "at %i: %li <= %li", i,
				  dhpb->dthpb_base + dhpb->dthpb_enoffs[i],
				  dhpb->dthpb_base + dhpb->dthpb_enoffs[i - 1]);
			return -1;
		}
	}

	if (dhpb->dthpb_noffs == 0 && dhpb->dthpb_nenoffs == 0) {
		dof_error(out, EINVAL, "USDT probe with zero tracepoints");
		return -1;
	}
	return 0;
}

static size_t
strings_len(const char *strtab, size_t count)
{
	size_t len = 0;

	for (; count > 0; count--) {
		size_t this_len = strlen(strtab) + 1;

		len += this_len;
		strtab += this_len;
	}
	return len;
}

static void
emit_probe(int out, dtrace_helper_probedesc_t *dhpb)
{
	int		i;
	dof_parsed_t	*msg;
	size_t		msg_size;
	char		*ptr;

	dt_dbg_dof("      Creating probe %s:%s:%s:%s\n", dhpb->dthpb_prov,
	    dhpb->dthpb_mod, dhpb->dthpb_func, dhpb->dthpb_name);

	if (validate_probe(out, dhpb) != 0)
		return;

	/*
	 * Compute the size of all the optional elements first, to fill out the
	 * flags.
	 */

	msg_size = offsetof(dof_parsed_t, probe.name) +
		   strlen(dhpb->dthpb_mod) + 1 +
		   strlen(dhpb->dthpb_func) + 1 +
		   strlen(dhpb->dthpb_name) + 1;

	msg = malloc(msg_size);
	if (!msg)
		goto oom;

	memset(msg, 0, msg_size);

	msg->size = msg_size;
	msg->type = DIT_PROBE;
	msg->probe.ntp = dhpb->dthpb_noffs + dhpb->dthpb_nenoffs;
	msg->probe.nargc = dhpb->dthpb_nargc;
	msg->probe.xargc = dhpb->dthpb_xargc;

	ptr = stpcpy(msg->probe.name, dhpb->dthpb_mod);
	ptr++;
	ptr = stpcpy(ptr, dhpb->dthpb_func);
	ptr++;
	strcpy(ptr, dhpb->dthpb_name);
	dof_parser_write_one(out, msg, msg_size);

	free(msg);

	/*
	 * Emit info on all native and translated args in turn.
	 *
	 * FIXME: this code is a bit repetitious.
	 *
	 * First native args (if any).
	 */

	if (dhpb->dthpb_nargc > 0) {
		size_t	nargs_size;

		nargs_size = strings_len(dhpb->dthpb_ntypes, dhpb->dthpb_nargc);
		msg_size = offsetof(dof_parsed_t, nargs.args) + nargs_size;

		msg = malloc(msg_size);
		if (!msg)
			goto oom;

		memset(msg, 0, msg_size);

		msg->size = msg_size;
		msg->type = DIT_ARGS_NATIVE;
		memcpy(msg->nargs.args, dhpb->dthpb_ntypes, nargs_size);
		dof_parser_write_one(out, msg, msg_size);

		free(msg);

		/* Then translated args. */

		if (dhpb->dthpb_xargc > 0) {
			size_t	xargs_size, map_size;

			xargs_size = strings_len(dhpb->dthpb_xtypes,
						 dhpb->dthpb_xargc);
			msg_size = offsetof(dof_parsed_t, xargs.args) +
				   xargs_size;

			msg = malloc(msg_size);
			if (!msg)
				goto oom;

			memset(msg, 0, msg_size);

			msg->size = msg_size;
			msg->type = DIT_ARGS_XLAT;
			memcpy(msg->xargs.args, dhpb->dthpb_xtypes, xargs_size);
			dof_parser_write_one(out, msg, msg_size);

			free(msg);

			/* Then the mapping table. */

			map_size = dhpb->dthpb_xargc * sizeof(int8_t);
			msg_size = offsetof(dof_parsed_t, argmap.argmap) +
				   map_size;

			msg = malloc(msg_size);
			if (!msg)
				goto oom;

			memset(msg, 0, msg_size);

			msg->size = msg_size;
			msg->type = DIT_ARGS_MAP;
			memcpy(msg->argmap.argmap, dhpb->dthpb_args, map_size);
			dof_parser_write_one(out, msg, msg_size);
			free(msg);
		}
	}

	/*
	 * Emit info on each tracepoint in turn.
	 */
	for (i = 0; i < dhpb->dthpb_noffs; i++)
		emit_tp(out, dhpb->dthpb_base, dhpb->dthpb_offs[i], 0);

	/*
	 * Then create a tracepoint for each is-enabled point.
	 *
	 * XXX original code looped over ntps here, which is noffs + enoffs.
	 * This seems surely wrong!
	 */
	for (i = 0; i < dhpb->dthpb_nenoffs; i++)
		emit_tp(out, dhpb->dthpb_base, dhpb->dthpb_enoffs[i], 1);

	return;
 oom:
	dof_error(out, ENOMEM, "Out of memory allocating %zi bytes for probe",
		  msg_size);
}

static void
emit_provider(int out, dof_helper_t *dhp,
    dof_hdr_t *dof, dof_sec_t *sec)
{
	uintptr_t		daddr = (uintptr_t)dof;
	uint32_t		*off, *enoff;
	char			*strtab;
	uint_t			i;
	void			*arg;

	dof_sec_t		*str_sec, *prb_sec, *arg_sec, *off_sec,
				*enoff_sec;
	dof_provider_t		*prov;

	dof_parsed_t		*provider_msg;
	size_t			provider_msg_size;

	dtrace_helper_probedesc_t	dhpb;

	prov = (dof_provider_t *)(uintptr_t)(daddr + sec->dofs_offset);
	str_sec = (dof_sec_t *)(uintptr_t)(daddr + dof->dofh_secoff +
					   prov->dofpv_strtab *
					   dof->dofh_secsize);
	prb_sec = (dof_sec_t *)(uintptr_t)(daddr + dof->dofh_secoff +
					   prov->dofpv_probes *
					   dof->dofh_secsize);
	arg_sec = (dof_sec_t *)(uintptr_t)(daddr + dof->dofh_secoff +
					   prov->dofpv_prargs *
					   dof->dofh_secsize);
	off_sec = (dof_sec_t *)(uintptr_t)(daddr + dof->dofh_secoff +
					   prov->dofpv_proffs *
					   dof->dofh_secsize);

	strtab = (char *)(uintptr_t)(daddr + str_sec->dofs_offset);
	off = (uint32_t *)(uintptr_t)(daddr + off_sec->dofs_offset);
	arg = (uint8_t *)(uintptr_t)(daddr + arg_sec->dofs_offset);
	enoff = NULL;

	/*
	 * See validate_probe().  Ignore is-enabled probes for DOF version 2:
	 * these probes rely on modifying the return value of the is-enabled
	 * probe, and using the modern approach of writing a 1 to the first
	 * argument will write to a nonexistent argument and cause, at best, a
	 * crash, at worst, memory corruption.  Avoiding parsing them ultimately
	 * avoids creating the system-level probes at all, avoiding the
	 * corruption at the cost of causing the probes to always return 0.
	 */
	if (dof->dofh_ident[DOF_ID_VERSION] >= DOF_VERSION_3 &&
	    prov->dofpv_prenoffs != DOF_SECT_NONE) {
		enoff_sec = (dof_sec_t *)(uintptr_t)
		  (daddr + dof->dofh_secoff +
		   prov->dofpv_prenoffs * dof->dofh_secsize);
		enoff = (uint32_t *)(uintptr_t)
		  (daddr + enoff_sec->dofs_offset);
	}

	dhpb.dthpb_prov = strtab + prov->dofpv_name;
	provider_msg_size = offsetof(dof_parsed_t, provider.name) +
	    strlen(dhpb.dthpb_prov) + 1;

	provider_msg = malloc(provider_msg_size);
	if (!provider_msg) {
		dof_error(out, ENOMEM, "Out of memory allocating probe");
		return;
	}
	memset(provider_msg, 0, provider_msg_size);

	provider_msg->size = provider_msg_size;
	provider_msg->type = DIT_PROVIDER;
	provider_msg->provider.nprobes = prb_sec->dofs_size / prb_sec->dofs_entsize;
	strcpy(provider_msg->provider.name, dhpb.dthpb_prov);
	dof_parser_write_one(out, provider_msg, provider_msg_size);

	/*
	 * Pass back info on the probes and their associated tracepoints.
	 */
	for (i = 0; i < provider_msg->provider.nprobes; i++) {
		dof_probe_t *probe;

		probe = (dof_probe_t *)(uintptr_t)(daddr +
						   prb_sec->dofs_offset +
						   i * prb_sec->dofs_entsize);

		dhpb.dthpb_mod = dhp->dofhp_mod;
		dhpb.dthpb_func = strtab + probe->dofpr_func;
		dhpb.dthpb_name = strtab + probe->dofpr_name;
		dhpb.dthpb_base = probe->dofpr_addr;
		dhpb.dthpb_offs = off + probe->dofpr_offidx;
		dhpb.dthpb_noffs = probe->dofpr_noffs;

		if (enoff != NULL) {
			dhpb.dthpb_enoffs = enoff + probe->dofpr_enoffidx;
			dhpb.dthpb_nenoffs = probe->dofpr_nenoffs;
		} else {
			dhpb.dthpb_enoffs = NULL;
			dhpb.dthpb_nenoffs = 0;
		}

		dhpb.dthpb_args = ((unsigned char *) arg) + probe->dofpr_argidx;
		dhpb.dthpb_nargc = probe->dofpr_nargc;
		dhpb.dthpb_xargc = probe->dofpr_xargc;
		dhpb.dthpb_ntypes = strtab + probe->dofpr_nargv;
		dhpb.dthpb_xtypes = strtab + probe->dofpr_xargv;

		emit_probe(out, &dhpb);
	}

	free(provider_msg);
}

void
dof_parse(int out, dof_helper_t *dhp, dof_hdr_t *dof)
{
	int			i, rv;
	uintptr_t		daddr = (uintptr_t)dof;
	int			count = 0;

	dt_dbg_dof("DOF 0x%p from helper {'%s', %p, %p}...\n",
		   dof, dhp ? dhp->dofhp_mod : "<none>", dhp, dof);

	rv = dof_slurp(out, dof, dhp->dofhp_addr);
	if (rv != 0) {
		dof_destroy(dhp, dof);
		return;
	}

	/*
	 * Look for helper providers, validate their descriptions, and
	 * parse them.
	 */
	if (dhp != NULL) {
		dt_dbg_dof("  DOF 0x%p Validating and parsing providers...\n", dof);

		for (i = 0; i < dof->dofh_secnum; i++) {
			dof_sec_t *sec;

			sec = (dof_sec_t *)(uintptr_t)
				(daddr + dof->dofh_secoff +
				 i * dof->dofh_secsize);

			if (sec->dofs_type != DOF_SECT_PROVIDER)
				continue;

			if (validate_provider(out, dof, sec) != 0) {
				dof_destroy(dhp, dof);
				return;
			}
			count++;
			emit_provider(out, dhp, dof, sec);
		}
	}

	/*
	 * If nothing was written, emit an empty result to wake up
	 * the caller.
	 */
	if (count == 0) {
		dof_parsed_t empty;

		memset(&empty, 0, sizeof(dof_parsed_t));

		empty.size = offsetof(dof_parsed_t, provider.name);
		empty.type = DIT_PROVIDER;
		empty.provider.nprobes = 0;
		dof_parser_write_one(out, &empty, empty.size);
	}

	dof_destroy(dhp, dof);
}
