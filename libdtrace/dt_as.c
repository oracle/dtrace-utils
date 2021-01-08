/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <sys/bitmap.h>
#include <stdlib.h>
#include <assert.h>

#include <dt_impl.h>
#include <dt_parser.h>
#include <dt_as.h>
#include <bpf_asm.h>
#include <port.h>

void
dt_irlist_create(dt_irlist_t *dlp)
{
	memset(dlp, 0, sizeof(dt_irlist_t));
	dlp->dl_label = 1;
}

void
dt_irlist_destroy(dt_irlist_t *dlp)
{
	dt_irnode_t *dip, *nip;

	for (dip = dlp->dl_list; dip != NULL; dip = nip) {
		nip = dip->di_next;
		free(dip);
	}
}

void
dt_irlist_append(dt_irlist_t *dlp, dt_irnode_t *dip)
{
	if (dlp->dl_last != NULL)
		dlp->dl_last->di_next = dip;
	else
		dlp->dl_list = dip;

	dlp->dl_last = dip;

	if (dip->di_label == DT_LBL_NONE || !BPF_IS_NOP(dip->di_instr))
		dlp->dl_len++; /* don't count forward refs in instr count */
}

uint_t
dt_irlist_label(dt_irlist_t *dlp)
{
	return dlp->dl_label++;
}

/*ARGSUSED*/
static int
dt_countvar(dt_idhash_t *dhp, dt_ident_t *idp, void *data)
{
	size_t *np = data;

	if (idp->di_flags & (DT_IDFLG_DIFR | DT_IDFLG_DIFW))
		(*np)++;		/* include variable in vartab */
	else if (idp->di_kind == DT_IDENT_AGG)
		(*np)++;		/* include variable in vartab */

	return 0;
}

/*ARGSUSED*/
static int
dt_copyvar(dt_idhash_t *dhp, dt_ident_t *idp, dtrace_hdl_t *dtp)
{
	dt_pcb_t	*pcb = dtp->dt_pcb;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dtrace_difv_t	*dvp;
	ssize_t		stroff;
	dt_node_t	dn;

	if (!(idp->di_flags & (DT_IDFLG_DIFR | DT_IDFLG_DIFW)) &&
	    idp->di_kind != DT_IDENT_AGG)
		return 0;		/* omit variable from vartab */

	dvp = &pcb->pcb_difo->dtdo_vartab[pcb->pcb_asvidx++];
	stroff = dt_strtab_insert(dtp->dt_ccstab, idp->di_name);

	if (stroff == -1L)
		longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	if (stroff > DIF_STROFF_MAX)
		longjmp(pcb->pcb_jmpbuf, EDT_STR2BIG);

	dvp->dtdv_name = (uint_t)stroff;
	dvp->dtdv_id = idp->di_id;
	dvp->dtdv_flags = 0;

	switch (idp->di_kind) {
	case DT_IDENT_AGG:
		dvp->dtdv_kind = DIFV_KIND_AGGREGATE;
		break;
	case DT_IDENT_ARRAY:
		dvp->dtdv_kind = DIFV_KIND_ARRAY;
		break;
	default:
		dvp->dtdv_kind = DIFV_KIND_SCALAR;
	}


	if (idp->di_flags & DT_IDFLG_LOCAL)
		dvp->dtdv_scope = DIFV_SCOPE_LOCAL;
	else if (idp->di_flags & DT_IDFLG_TLS)
		dvp->dtdv_scope = DIFV_SCOPE_THREAD;
	else
		dvp->dtdv_scope = DIFV_SCOPE_GLOBAL;

	dvp->dtdv_insn_from = 0;
	dvp->dtdv_insn_to = dlp->dl_len - 1;

	if (idp->di_flags & DT_IDFLG_DIFR)
		dvp->dtdv_flags |= DIFV_F_REF;
	if (idp->di_flags & DT_IDFLG_DIFW)
		dvp->dtdv_flags |= DIFV_F_MOD;

	memset(&dn, 0, sizeof(dn));
	dt_node_type_assign(&dn, idp->di_ctfp, idp->di_type);
	dt_node_diftype(pcb->pcb_hdl, &dn, &dvp->dtdv_type);

	idp->di_flags &= ~(DT_IDFLG_DIFR | DT_IDFLG_DIFW);
	return 0;
}

#ifdef FIXME
/*
 * Rewrite the xlate/xlarg instruction at dtdo_buf[i] so that the instruction's
 * xltab index reflects the offset 'xi' of the assigned dtdo_xlmtab[] location.
 * We track the cumulative references to translators and members in the pcb's
 * pcb_asxrefs[] array, a two-dimensional array of bitmaps indexed by the
 * global translator id and then by the corresponding translator member id.
 */
static void
dt_as_xlate(dt_pcb_t *pcb, dtrace_difo_t *dp,
    uint_t i, uint_t xi, dt_node_t *dnp)
{
	dtrace_hdl_t *dtp = pcb->pcb_hdl;
	dt_xlator_t *dxp = dnp->dn_membexpr->dn_xlator;

	assert(i < dp->dtdo_len);
	assert(xi < dp->dtdo_xlmlen);

	assert(dnp->dn_kind == DT_NODE_MEMBER);
	assert(dnp->dn_membexpr->dn_kind == DT_NODE_XLATOR);

	assert(dxp->dx_id < dtp->dt_xlatorid);
	assert(dnp->dn_membid < dxp->dx_nmembers);

	if (pcb->pcb_asxrefs == NULL) {
		pcb->pcb_asxreflen = dtp->dt_xlatorid;
		pcb->pcb_asxrefs = dt_calloc(dtp, pcb->pcb_asxreflen,
						  sizeof(ulong_t *));
		if (pcb->pcb_asxrefs == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	}

	if (pcb->pcb_asxrefs[dxp->dx_id] == NULL) {
		pcb->pcb_asxrefs[dxp->dx_id] =
		    dt_zalloc(dtp, BT_SIZEOFMAP(dxp->dx_nmembers));
		if (pcb->pcb_asxrefs[dxp->dx_id] == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	}

	dp->dtdo_buf[i] = DIF_INSTR_XLATE(
	    DIF_INSTR_OP(dp->dtdo_buf[i]), xi, DIF_INSTR_RD(dp->dtdo_buf[i]));

	BT_SET(pcb->pcb_asxrefs[dxp->dx_id], dnp->dn_membid);
	dp->dtdo_xlmtab[xi] = dnp;
}
#endif

static void
dt_as_undef(const dt_ident_t *idp, uint_t offset)
{
	const char *kind, *mark = (idp->di_flags & DT_IDFLG_USER) ? "``" : "`";
	const dtrace_syminfo_t *dts = idp->di_data;

	if (idp->di_flags & DT_IDFLG_USER)
		kind = "user";
	else
		kind = "kernel";

	yylineno = idp->di_lineno;

	xyerror(D_ASRELO, "relocation remains against %s symbol %s%s%s "
		"(offset 0x%x)\n", kind, dts->object, mark, dts->name, offset);
}

dtrace_difo_t *
dt_as(dt_pcb_t *pcb)
{
	dtrace_hdl_t *dtp = pcb->pcb_hdl;
	dt_irlist_t *dlp = &pcb->pcb_ir;
	uint_t *labels = NULL;
	dt_irnode_t *dip;
	dtrace_difo_t *dp;
	dt_ident_t *idp;

	size_t n = 0;
	uint_t i;

	uint_t kmask, kbits, umask, ubits;
	uint_t brel = 0, krel = 0, urel = 0, xlrefs = 0;

	/*
	 * Select bitmasks based upon the desired symbol linking policy.  We
	 * test (di_extern->di_flags & xmask) == xbits to determine if the
	 * symbol should have a relocation entry generated in the loop below.
	 *
	 * DT_LINK_KERNEL = kernel symbols static, user symbols dynamic
	 * DT_LINK_DYNAMIC = all symbols dynamic
	 * DT_LINK_STATIC = all symbols static
	 *
	 * By 'static' we mean that we use the symbol's value at compile-time
	 * in the final DIF.  By 'dynamic' we mean that we create a relocation
	 * table entry for the symbol's value so it can be relocated later.
	 */
	switch (dtp->dt_linkmode) {
	case DT_LINK_KERNEL:
		kmask = 0;
		kbits = -1u;
		umask = DT_IDFLG_USER;
		ubits = DT_IDFLG_USER;
		break;
	case DT_LINK_DYNAMIC:
		kmask = DT_IDFLG_USER;
		kbits = 0;
		umask = DT_IDFLG_USER;
		ubits = DT_IDFLG_USER;
		break;
	case DT_LINK_STATIC:
		kmask = umask = 0;
		kbits = ubits = -1u;
		break;
	default:
		xyerror(D_UNKNOWN, "internal error -- invalid link mode %u\n",
		    dtp->dt_linkmode);
	}

	assert(pcb->pcb_difo == NULL);
	pcb->pcb_difo = dt_zalloc(dtp, sizeof(dtrace_difo_t));

	if ((dp = pcb->pcb_difo) == NULL)
		longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);

	dp->dtdo_buf = dt_calloc(dtp, dlp->dl_len, sizeof(struct bpf_insn));
	if (dp->dtdo_buf == NULL)
		longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);

	labels = dt_calloc(dtp, dlp->dl_label, sizeof(uint_t));
	if (labels == NULL)
		longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);

	/*
	 * Make an initial pass through the instruction list, filling in the
	 * instruction buffer with valid instructions and skipping labeled nops.
	 * While doing this, we also fill in our labels[] translation table
	 * and we count up the number of relocation table entries we will need.
	 */
	for (i = 0, dip = dlp->dl_list; dip != NULL; dip = dip->di_next) {
		if (dip->di_label != DT_LBL_NONE)
			labels[dip->di_label] = i;

		if (dip->di_label == DT_LBL_NONE || !BPF_IS_NOP(dip->di_instr))
			dp->dtdo_buf[i++] = dip->di_instr;

		if ((idp = dip->di_extern) == NULL)
			continue; /* no external references needed */

		switch (dip->di_instr.code) {
		case BPF_ST | BPF_MEM | BPF_W:		/* stw */
		case BPF_ST | BPF_MEM | BPF_DW:		/* stdw */
		case BPF_ALU64 | BPF_MOV | BPF_K:	/* mov */
			if (idp->di_flags & DT_IDFLG_BPF)
				brel++;
			else
				goto fail;
			break;
		case BPF_LD | BPF_IMM | BPF_DW:	/* lddw */
			if (idp->di_flags & DT_IDFLG_BPF)
				brel++;
			else if ((idp->di_flags & kmask) == kbits)
				krel++;
			else if ((idp->di_flags & umask) == ubits)
				urel++;
			break;
		case BPF_JMP | BPF_CALL:	/* call */
			if (dip->di_instr.src_reg == BPF_PSEUDO_CALL &&
			    (idp->di_flags & DT_IDFLG_BPF))
				brel++;
			else
				goto fail;
			break;
		default:
fail:
			xyerror(D_UNKNOWN, "unexpected asm relocation for "
				"opcode 0x%x @%d, id %s\n",
				dip->di_instr.code, i - 1, idp->di_name);
		}

#ifdef FIXME
		switch (DIF_INSTR_OP(dip->di_instr)) {
		case DIF_OP_SETX:
			idp = dip->di_extern;
			if ((idp->di_flags & kmask) == kbits)
				krel++;
			else if ((idp->di_flags & umask) == ubits)
				urel++;
			break;
		case DIF_OP_XLATE:
		case DIF_OP_XLARG:
			xlrefs++;
			break;
		default:
			xyerror(D_UNKNOWN, "unexpected asm relocation for "
			    "opcode 0x%x\n", DIF_INSTR_OP(dip->di_instr));
		}
#endif
	}

	assert(i == dlp->dl_len);
	dp->dtdo_len = dlp->dl_len;

	/*
	 * Make a second pass through the instructions, relocating each branch
	 * target (a label ID) to the relative location of the label and noting
	 * any instruction-specific flags such as DIFOFLG_DESTRUCTIVE.
	 */
	for (i = 0; i < dp->dtdo_len; i++) {
		struct bpf_insn instr = dp->dtdo_buf[i];
		uint_t op = BPF_OP(instr.code);

		/* We only care about jump instructions. */
		if (BPF_CLASS(instr.code) != BPF_JMP)
			continue;

		/* We ignore NOP (jmp 0). */
		if (BPF_IS_NOP(instr))
			continue;

		/* We ignore function calls and function exits. */
		if (op == BPF_CALL || op == BPF_EXIT)
			continue;
#ifdef FIXME
		if (op == DIF_OP_CALL) {
			if (DIF_INSTR_SUBR(instr) == DIF_SUBR_COPYOUT ||
			    DIF_INSTR_SUBR(instr) == DIF_SUBR_COPYOUTSTR)
				dp->dtdo_flags |= DIFOFLG_DESTRUCTIVE;
			continue;
		}
#endif

		assert(instr.off < dlp->dl_label);
		/*
		 * BPF jump instructions use an offset from the *following*
		 * instructions, so we need to subtract one extra instruction
		 * from the delta between the jump and the labelled target.
		 */
		dp->dtdo_buf[i].off = labels[instr.off] - i - 1;
	}

	pcb->pcb_asvidx = 0;

	/*
	 * Allocate memory for the appropriate number of variable records and
	 * then fill in each variable record.  As we populate the variable
	 * table we insert the corresponding variable names into the strtab.
	 */
	dt_idhash_iter(dtp->dt_tls, dt_countvar, &n);
	dt_idhash_iter(dtp->dt_aggs, dt_countvar, &n);
	dt_idhash_iter(dtp->dt_globals, dt_countvar, &n);
	dt_idhash_iter(pcb->pcb_locals, dt_countvar, &n);

	if (n != 0) {
		dp->dtdo_vartab = dt_calloc(dtp, n, sizeof(dtrace_difv_t));
		dp->dtdo_varlen = (uint32_t)n;

		if (dp->dtdo_vartab == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);

		dt_idhash_iter(dtp->dt_tls, (dt_idhash_f *)dt_copyvar, dtp);
		dt_idhash_iter(dtp->dt_aggs, (dt_idhash_f *)dt_copyvar, dtp);
		dt_idhash_iter(dtp->dt_globals, (dt_idhash_f *)dt_copyvar, dtp);
		dt_idhash_iter(pcb->pcb_locals, (dt_idhash_f *)dt_copyvar, dtp);
	}

	/*
	 * Allocate memory for the appropriate number of relocation table
	 * entries based upon our kernel and user counts from the first pass.
	 */
	if (brel != 0) {
		dp->dtdo_breltab = dt_calloc(dtp, brel, sizeof(dof_relodesc_t));
		dp->dtdo_brelen = brel;

		if (dp->dtdo_breltab == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	}

	if (krel != 0) {
		dp->dtdo_kreltab = dt_calloc(dtp, krel, sizeof(dof_relodesc_t));
		dp->dtdo_krelen = krel;

		if (dp->dtdo_kreltab == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	}

	if (urel != 0) {
		dp->dtdo_ureltab = dt_calloc(dtp, urel, sizeof(dof_relodesc_t));
		dp->dtdo_urelen = urel;

		if (dp->dtdo_ureltab == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	}

	if (xlrefs != 0) {
		dp->dtdo_xlmtab = dt_calloc(dtp, xlrefs, sizeof(dt_node_t *));
		dp->dtdo_xlmlen = xlrefs;

		if (dp->dtdo_xlmtab == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	}

	/*
	 * If any relocations are needed, make another pass through the
	 * instruction list and fill in the relocation table entries.
	 */
	if (brel + krel + urel + xlrefs > 0) {
		uint_t knodef = pcb->pcb_cflags & DTRACE_C_KNODEF;
		uint_t unodef = pcb->pcb_cflags & DTRACE_C_UNODEF;

		dof_relodesc_t *brp = dp->dtdo_breltab;
		dof_relodesc_t *krp = dp->dtdo_kreltab;
		dof_relodesc_t *urp = dp->dtdo_ureltab;
		dt_node_t **xlp = dp->dtdo_xlmtab;

		i = 0; /* dtdo_buf[] index */

		for (dip = dlp->dl_list; dip != NULL; dip = dip->di_next) {
			dof_relodesc_t *rp;
			ssize_t soff;
			uint_t nodef;

			if (dip->di_label != DT_LBL_NONE &&
			    BPF_IS_NOP(dip->di_instr))
				continue; /* skip label declarations */

			i++; /* advance dtdo_buf[] index */

#ifdef FIXME
			if (DIF_INSTR_OP(dip->di_instr) == DIF_OP_XLATE ||
			    DIF_INSTR_OP(dip->di_instr) == DIF_OP_XLARG) {
				assert(BPF_EQUAL(dp->dtdo_buf[i - 1],
						 dip->di_instr));
				dt_as_xlate(pcb, dp, i - 1, (uint_t)
				    (xlp++ - dp->dtdo_xlmtab), dip->di_extern);
				continue;
			}
#endif

			if ((idp = dip->di_extern) == NULL)
				continue; /* no relocation entry needed */

			if (idp->di_flags & DT_IDFLG_BPF) {
				nodef = 1;
				rp = brp++;
			} else if ((idp->di_flags & kmask) == kbits) {
				nodef = knodef;
				rp = krp++;
			} else if ((idp->di_flags & umask) == ubits) {
				nodef = unodef;
				rp = urp++;
			} else
				continue;

			if (!nodef)
				dt_as_undef(idp, i);

			if (dip->di_instr.code ==
					(BPF_LD | BPF_IMM | BPF_DW))
				rp->dofr_type = R_BPF_64_64;
			else if (dip->di_instr.code ==
					(BPF_ST | BPF_MEM | BPF_W))
				rp->dofr_type = R_BPF_64_32;
			else if (dip->di_instr.code ==
					(BPF_ST | BPF_MEM | BPF_DW))
				rp->dofr_type = R_BPF_64_32;
			else if (dip->di_instr.code ==
					(BPF_ALU64 | BPF_MOV | BPF_K))
				rp->dofr_type = R_BPF_64_32;
			else if (dip->di_instr.code ==
					(BPF_JMP | BPF_CALL) &&
				 dip->di_instr.src_reg == BPF_PSEUDO_CALL)
				rp->dofr_type = R_BPF_64_32;
			else
				xyerror(D_UNKNOWN, "unexpected asm relocation "
					"for opcode 0x%x (@%d, %s)\n",
					dip->di_instr.code, i - 1,
					idp->di_name);

			soff = dt_strtab_insert(dtp->dt_ccstab, idp->di_name);

			if (soff == -1L)
				longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
			if (soff > DIF_STROFF_MAX)
				longjmp(pcb->pcb_jmpbuf, EDT_STR2BIG);

			if ((idp->di_flags & DT_IDFLG_BPF) != 0) {
				/*
				 * Relocation for BPF identifier.
				 */
				rp->dofr_name = (dof_stridx_t)soff;
				rp->dofr_offset = (i - 1) * sizeof(uint64_t);

				/*
				 * BPF built-in functions (predicate and
				 * program) are special because they are part
				 * of the executable code we are processing
				 * here.  Set the value in the relocation entry
				 * to be the instruction offset of the function
				 * *and* update the call instruction with the
				 * correct offset delta.
				 */
				if (idp->di_kind == DT_IDENT_FUNC) {
					assert(idp->di_id < dlp->dl_label);
					rp->dofr_data = labels[idp->di_id];
					dp->dtdo_buf[i - 1].imm =
							labels[idp->di_id] - i;
				} else
					rp->dofr_data = idp->di_id;
			} else {
				/*
				 * Relocation for a regular external symbol.
				 */
				assert(idp->di_data != NULL);
				rp->dofr_name = (dof_stridx_t)soff;
				rp->dofr_offset = (i - 1) * sizeof(uint64_t);
				rp->dofr_data =
					((dtrace_syminfo_t *)idp->di_data)->id;
			}
		}

		assert(brp == dp->dtdo_breltab + dp->dtdo_brelen);
		assert(krp == dp->dtdo_kreltab + dp->dtdo_krelen);
		assert(urp == dp->dtdo_ureltab + dp->dtdo_urelen);
		assert(xlp == dp->dtdo_xlmtab + dp->dtdo_xlmlen);
		assert(i == dp->dtdo_len);
	}

	dt_free(dtp, labels);		/* Done with labels. */

	/*
	 * Allocate memory for the compiled string table and then copy the
	 * chunks from the string table into the final string buffer for the
	 * DIFO we are constructing.
	 *
	 * We keep the string table around (dtp->dt_csstab) because further
	 * compilations may add more strings.  We will load a consolidated
	 * compiled string table into a strtab BPF map so it can be used ny
	 * all BPF programs we will be loading.  Since each DIFO will have a
	 * string table that comprises the strings of all DIFOs before it along
	 * with any new ones, we simply assign the latest (and most complete)
	 * string table to dtp->dt_strtab (and the size as dtp->dt_strlen), so
	 * that when we are ready to load and execute programs, we know we have
	 * the latest and greatest string table to work with.
	 *
	 * Note that this means that we should *not* free dtp->dt_strtab later
	 * because it will be free'd when the DIFO is destroyed.
	 */
	dp->dtdo_strlen = dt_strtab_size(dtp->dt_ccstab);
	if (dp->dtdo_strlen > 0) {
		dp->dtdo_strtab = dt_zalloc(dtp, dp->dtdo_strlen);
		if (dp->dtdo_strtab == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);

		dt_strtab_write(dtp->dt_ccstab,
				(dt_strtab_write_f *)dt_strtab_copystr,
				dp->dtdo_strtab);

		dtp->dt_strtab = dp->dtdo_strtab;
		dtp->dt_strlen = dp->dtdo_strlen;
	} else
		dp->dtdo_strtab = NULL;

	/*
	 *
	 * Clear pcb_difo * now that the assembler has completed successfully.
	 */
	pcb->pcb_difo = NULL;

	/*
	 * Fill in the trace data record length.  This is used to determine the
	 * size of the scratch space where the trace data will be assembler
	 * before it is written to the output buffer.
	 */
	dp->dtdo_reclen = pcb->pcb_bufoff;
	if (dp->dtdo_reclen > dtp->dt_maxreclen)
		dtp->dt_maxreclen = dp->dtdo_reclen;

	return dp;
}

dtrace_difo_t *
dt_difo_copy(dtrace_hdl_t *dtp, const dtrace_difo_t *odp)
{
	dtrace_difo_t	*dp;

	dp = dt_zalloc(dtp, sizeof(dtrace_difo_t));
	if (dp == NULL)
		goto no_mem;

#define DIFO_COPY_DATA(dtp, odp, dp, len, ptr) \
	do { \
		if ((odp)->len > 0) { \
			size_t	tsiz = sizeof(typeof((dp)->ptr[0])); \
			\
			(dp)->len = (odp)->len; \
			(dp)->ptr = dt_calloc((dtp), (dp)->len, tsiz); \
			if ((dp)->ptr == NULL) \
				goto no_mem; \
			\
			memcpy((dp)->ptr, (odp)->ptr, (dp)->len * tsiz); \
		} \
	} while (0)

	DIFO_COPY_DATA(dtp, odp, dp, dtdo_len, dtdo_buf);
	DIFO_COPY_DATA(dtp, odp, dp, dtdo_strlen, dtdo_strtab);
	DIFO_COPY_DATA(dtp, odp, dp, dtdo_varlen, dtdo_vartab);
	DIFO_COPY_DATA(dtp, odp, dp, dtdo_brelen, dtdo_breltab);
	DIFO_COPY_DATA(dtp, odp, dp, dtdo_krelen, dtdo_kreltab);
	DIFO_COPY_DATA(dtp, odp, dp, dtdo_urelen, dtdo_ureltab);

	dp->dtdo_ddesc = dt_datadesc_hold(odp->dtdo_ddesc);
	dp->dtdo_flags = odp->dtdo_flags;

	return dp;

no_mem:
	dt_free(dtp, dp->dtdo_buf);
	dt_free(dtp, dp->dtdo_strtab);
	dt_free(dtp, dp->dtdo_vartab);
	dt_free(dtp, dp->dtdo_breltab);
	dt_free(dtp, dp->dtdo_kreltab);
	dt_free(dtp, dp->dtdo_ureltab);
	dt_free(dtp, dp);

	dt_set_errno(dtp, EDT_NOMEM);
	return NULL;
}
