/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2019, Oracle and/or its affiliates. All rights reserved.
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

void
dt_irlist_create(dt_irlist_t *dlp)
{
	memset(dlp, 0, sizeof (dt_irlist_t));
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
	return (dlp->dl_label++);
}

/*ARGSUSED*/
static int
dt_countvar(dt_idhash_t *dhp, dt_ident_t *idp, void *data)
{
	size_t *np = data;

	if (idp->di_flags & (DT_IDFLG_DIFR | DT_IDFLG_DIFW))
		(*np)++; /* include variable in vartab */

	return (0);
}

/*ARGSUSED*/
static int
dt_copyvar(dt_idhash_t *dhp, dt_ident_t *idp, void *data)
{
	dt_pcb_t *pcb = data;
	dtrace_difv_t *dvp;
	ssize_t stroff;
	dt_node_t dn;

	if (!(idp->di_flags & (DT_IDFLG_DIFR | DT_IDFLG_DIFW)))
		return (0); /* omit variable from vartab */

	dvp = &pcb->pcb_difo->dtdo_vartab[pcb->pcb_asvidx++];
	stroff = dt_strtab_insert(pcb->pcb_strtab, idp->di_name);

	if (stroff == -1L)
		longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	if (stroff > DIF_STROFF_MAX)
		longjmp(pcb->pcb_jmpbuf, EDT_STR2BIG);

	dvp->dtdv_name = (uint_t)stroff;
	dvp->dtdv_id = idp->di_id;
	dvp->dtdv_flags = 0;

	dvp->dtdv_kind = (idp->di_kind == DT_IDENT_ARRAY) ?
	    DIFV_KIND_ARRAY : DIFV_KIND_SCALAR;

	if (idp->di_flags & DT_IDFLG_LOCAL)
		dvp->dtdv_scope = DIFV_SCOPE_LOCAL;
	else if (idp->di_flags & DT_IDFLG_TLS)
		dvp->dtdv_scope = DIFV_SCOPE_THREAD;
	else
		dvp->dtdv_scope = DIFV_SCOPE_GLOBAL;

	if (idp->di_flags & DT_IDFLG_DIFR)
		dvp->dtdv_flags |= DIFV_F_REF;
	if (idp->di_flags & DT_IDFLG_DIFW)
		dvp->dtdv_flags |= DIFV_F_MOD;

	memset(&dn, 0, sizeof (dn));
	dt_node_type_assign(&dn, idp->di_ctfp, idp->di_type);
	dt_node_diftype(pcb->pcb_hdl, &dn, &dvp->dtdv_type);

	idp->di_flags &= ~(DT_IDFLG_DIFR | DT_IDFLG_DIFW);
	return (0);
}

static ssize_t
dt_copystr(const char *s, size_t n, size_t off, dt_pcb_t *pcb)
{
	memcpy(pcb->pcb_difo->dtdo_strtab + off, s, n);
	return (n);
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
		pcb->pcb_asxrefs =
		    dt_zalloc(dtp, sizeof (ulong_t *) * pcb->pcb_asxreflen);
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
	pcb->pcb_difo = dt_zalloc(dtp, sizeof (dtrace_difo_t));

	if ((dp = pcb->pcb_difo) == NULL)
		longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);

	dp->dtdo_buf = dt_alloc(dtp, sizeof (struct bpf_insn) * dlp->dl_len);

	if (dp->dtdo_buf == NULL)
		longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);

	if ((labels = dt_alloc(dtp, sizeof (uint_t) * dlp->dl_label)) == NULL)
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
			else
				goto fail;
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
	 * label to the index of the final instruction in the buffer and noting
	 * any other instruction-specific DIFO flags such as dtdo_destructive.
	 */
	for (i = 0; i < dp->dtdo_len; i++) {
		struct bpf_insn instr = dp->dtdo_buf[i];
		uint_t op = BPF_OP(instr.code);

		/* We only care about jump instructions. */
		if (BPF_CLASS(instr.code) != BPF_JMP)
			continue;

		/* We ignore function calls and function exits. */
		if (op == BPF_CALL || op == BPF_EXIT)
			continue;
#ifdef FIXME
		if (op == DIF_OP_CALL) {
			if (DIF_INSTR_SUBR(instr) == DIF_SUBR_COPYOUT ||
			    DIF_INSTR_SUBR(instr) == DIF_SUBR_COPYOUTSTR)
				dp->dtdo_destructive = 1;
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
	(void) dt_idhash_iter(dtp->dt_tls, dt_countvar, &n);
	(void) dt_idhash_iter(dtp->dt_globals, dt_countvar, &n);
	(void) dt_idhash_iter(pcb->pcb_locals, dt_countvar, &n);

	if (n != 0) {
		dp->dtdo_vartab = dt_alloc(dtp, n * sizeof (dtrace_difv_t));
		dp->dtdo_varlen = (uint32_t)n;

		if (dp->dtdo_vartab == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);

		(void) dt_idhash_iter(dtp->dt_tls, dt_copyvar, pcb);
		(void) dt_idhash_iter(dtp->dt_globals, dt_copyvar, pcb);
		(void) dt_idhash_iter(pcb->pcb_locals, dt_copyvar, pcb);
	}

	/*
	 * Allocate memory for the appropriate number of relocation table
	 * entries based upon our kernel and user counts from the first pass.
	 */
	if (brel != 0) {
		dp->dtdo_breltab = dt_alloc(dtp,
					    brel * sizeof (dof_relodesc_t));
		dp->dtdo_brelen = brel;

		if (dp->dtdo_breltab == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	}

	if (krel != 0) {
		dp->dtdo_kreltab = dt_alloc(dtp,
					    krel * sizeof (dof_relodesc_t));
		dp->dtdo_krelen = krel;

		if (dp->dtdo_kreltab == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	}

	if (urel != 0) {
		dp->dtdo_ureltab = dt_alloc(dtp,
					    urel * sizeof (dof_relodesc_t));
		dp->dtdo_urelen = urel;

		if (dp->dtdo_ureltab == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	}

	if (xlrefs != 0) {
		dp->dtdo_xlmtab = dt_zalloc(dtp, sizeof (dt_node_t *) * xlrefs);
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

			soff = dt_strtab_insert(pcb->pcb_strtab, idp->di_name);

			if (soff == -1L)
				longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
			if (soff > DIF_STROFF_MAX)
				longjmp(pcb->pcb_jmpbuf, EDT_STR2BIG);

			assert(idp->di_data != NULL);
			rp->dofr_name = (dof_stridx_t)soff;
			rp->dofr_offset = (i - 1) * sizeof (uint64_t);
			rp->dofr_data = ((dtrace_syminfo_t *)idp->di_data)->id;

			/*
			 * Crazy magic lies ahead...
			 *
			 * Due to the fact that dt_predicate and dt_program are
			 * generated in the same instruction list as the probe
			 * trampoline code, we have encoded a reference to the
			 * start of each of those functions using the label
			 * system (while other identifiers are real references
			 * to external symbols).
			 *
			 * This means we need to differentiate between the two
			 * when dealing with relocations.  We make use of the
			 * fact that regular external symbols store the same id
			 * in the identifier as they do in the syminfo.  For
			 * our two special functions (dt_predicate and
			 * dt_program), the syminfo one will be DT_IDENT_UNDEF
			 * while the identifier id will be the label id.
			 *
			 * So, when we encounter one of our special relocations
			 * we update the value of the relocation to be the
			 * instruction offset of the function *and* we patch
			 * the call instruction with the correct offset delta.
			 */
			if (rp->dofr_data == DT_IDENT_UNDEF &&
			    idp->di_id != DT_IDENT_UNDEF) {
				assert(idp->di_id < dlp->dl_label);
				rp->dofr_data = labels[idp->di_id];
				dp->dtdo_buf[i - 1].imm =
							labels[idp->di_id] - i;
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
	 * chunks from the string table into the final string buffer.
	 */
	if ((n = dt_strtab_size(pcb->pcb_strtab)) != 0) {
		if ((dp->dtdo_strtab = dt_alloc(dtp, n)) == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);

		(void) dt_strtab_write(pcb->pcb_strtab,
		    (dt_strtab_write_f *)dt_copystr, pcb);
		dp->dtdo_strlen = (uint32_t)n;
	}

	/*
	 * Fill in the DIFO return type from the type associated with the
	 * node saved in pcb_dret, and then clear pcb_difo and pcb_dret
	 * now that the assembler has completed successfully.
	 */
	dt_node_diftype(dtp, pcb->pcb_dret, &dp->dtdo_rtype);
	pcb->pcb_difo = NULL;
	pcb->pcb_dret = NULL;

	/* Track the orignal type. */
	dp->orig_dtdo_rtype = dp->dtdo_rtype;

	return (dp);
}
