/*
 * FILE:	dtrace_dif.c
 * DESCRIPTION:	Dynamic Tracing: DIF object functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/hardirq.h>
#include <linux/slab.h>

#include "dtrace.h"

size_t				dtrace_global_maxsize = 16 * 1024;

static uint64_t			dtrace_vtime_references;

static int dtrace_difo_err(uint_t pc, const char *format, ...)
{
	char	buf[256];

	if (dtrace_err_verbose) {
		va_list	alist;
		size_t	len = strlen(format);

		pr_err("dtrace DIF object error: [%u]: ", pc);

		if (len >= 256 - sizeof(KERN_ERR)) {
			pr_err("<invalid format string>");
			return 1;
		}

		memcpy(buf, KERN_ERR, sizeof(KERN_ERR));
		memcpy(buf + sizeof(KERN_ERR), format, len);

		va_start(alist, format);
		vprintk(buf, alist);
		va_end(alist);
	}

	return 1;
}

/*
 * Validate a DTrace DIF object by checking the IR instructions.  The following
 * rules are currently enforced by dtrace_difo_validate():
 *
 * 1. Each instruction must have a valid opcode
 * 2. Each register, string, variable, or subroutine reference must be valid
 * 3. No instruction can modify register %r0 (must be zero)
 * 4. All instruction reserved bits must be set to zero
 * 5. The last instruction must be a "ret" instruction
 * 6. All branch targets must reference a valid instruction _after_ the branch
 */
int dtrace_difo_validate(dtrace_difo_t *dp, dtrace_vstate_t *vstate,
			 uint_t nregs, const cred_t *cr)
{
	int	err = 0, i;
	int	(*efunc)(uint_t pc, const char *, ...) = dtrace_difo_err;
	int	kcheckload;
	uint_t	pc;

	kcheckload = cr == NULL ||
		     (vstate->dtvs_state->dts_cred.dcr_visible &
		      DTRACE_CRV_KERNEL) == 0;

	dp->dtdo_destructive = 0;

	for (pc = 0; pc < dp->dtdo_len && err == 0; pc++) {
		dif_instr_t	instr = dp->dtdo_buf[pc];
		uint_t		r1 = DIF_INSTR_R1(instr);
		uint_t		r2 = DIF_INSTR_R2(instr);
		uint_t		rd = DIF_INSTR_RD(instr);
		uint_t		rs = DIF_INSTR_RS(instr);
		uint_t		label = DIF_INSTR_LABEL(instr);
		uint_t		v = DIF_INSTR_VAR(instr);
		uint_t		subr = DIF_INSTR_SUBR(instr);
		uint_t		type = DIF_INSTR_TYPE(instr);
		uint_t		op = DIF_INSTR_OP(instr);

		switch (op) {
		case DIF_OP_OR:
		case DIF_OP_XOR:
		case DIF_OP_AND:
		case DIF_OP_SLL:
		case DIF_OP_SRL:
		case DIF_OP_SRA:
		case DIF_OP_SUB:
		case DIF_OP_ADD:
		case DIF_OP_MUL:
		case DIF_OP_SDIV:
		case DIF_OP_UDIV:
		case DIF_OP_SREM:
		case DIF_OP_UREM:
		case DIF_OP_COPYS:
			if (r1 >= nregs)
				err += efunc(pc, "invalid register %u\n", r1);
			if (r2 >= nregs)
				err += efunc(pc, "invalid register %u\n", r2);
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			if (rd == 0)
				err += efunc(pc, "cannot write to %r0\n");
			break;
		case DIF_OP_NOT:
		case DIF_OP_MOV:
		case DIF_OP_ALLOCS:
			if (r1 >= nregs)
				err += efunc(pc, "invalid register %u\n", r1);
			if (r2 != 0)
				err += efunc(pc, "non-zero reserved bits\n");
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			if (rd == 0)
				err += efunc(pc, "cannot write to %r0\n");
			break;
		case DIF_OP_LDSB:
		case DIF_OP_LDSH:
		case DIF_OP_LDSW:
		case DIF_OP_LDUB:
		case DIF_OP_LDUH:
		case DIF_OP_LDUW:
		case DIF_OP_LDX:
			if (r1 >= nregs)
				err += efunc(pc, "invalid register %u\n", r1);
			if (r2 != 0)
				err += efunc(pc, "non-zero reserved bits\n");
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			if (rd == 0)
				err += efunc(pc, "cannot write to %r0\n");
			if (kcheckload)
				dp->dtdo_buf[pc] = DIF_INSTR_LOAD(
							op + DIF_OP_RLDSB -
							     DIF_OP_LDSB,
							r1, rd);
			break;
		case DIF_OP_RLDSB:
		case DIF_OP_RLDSH:
		case DIF_OP_RLDSW:
		case DIF_OP_RLDUB:
		case DIF_OP_RLDUH:
		case DIF_OP_RLDUW:
		case DIF_OP_RLDX:
			if (r1 >= nregs)
				err += efunc(pc, "invalid register %u\n", r1);
			if (r2 != 0)
				err += efunc(pc, "non-zero reserved bits\n");
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			if (rd == 0)
				err += efunc(pc, "cannot write to %r0\n");
			break;
		case DIF_OP_ULDSB:
		case DIF_OP_ULDSH:
		case DIF_OP_ULDSW:
		case DIF_OP_ULDUB:
		case DIF_OP_ULDUH:
		case DIF_OP_ULDUW:
		case DIF_OP_ULDX:
			if (r1 >= nregs)
				err += efunc(pc, "invalid register %u\n", r1);
			if (r2 != 0)
				err += efunc(pc, "non-zero reserved bits\n");
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			if (rd == 0)
				err += efunc(pc, "cannot write to %r0\n");
			break;
		case DIF_OP_STB:
		case DIF_OP_STH:
		case DIF_OP_STW:
		case DIF_OP_STX:
			if (r1 >= nregs)
				err += efunc(pc, "invalid register %u\n", r1);
			if (r2 != 0)
				err += efunc(pc, "non-zero reserved bits\n");
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			if (rd == 0)
				err += efunc(pc, "cannot write to 0 address\n");
			break;
		case DIF_OP_CMP:
		case DIF_OP_SCMP:
			if (r1 >= nregs)
				err += efunc(pc, "invalid register %u\n", r1);
			if (r2 >= nregs)
				err += efunc(pc, "invalid register %u\n", r2);
			if (rd != 0)
				err += efunc(pc, "non-zero reserved bits\n");
			break;
		case DIF_OP_TST:
			if (r1 >= nregs)
				err += efunc(pc, "invalid register %u\n", r1);
			if (r2 != 0 || rd != 0)
				err += efunc(pc, "non-zero reserved bits\n");
			break;
		case DIF_OP_BA:
		case DIF_OP_BE:
		case DIF_OP_BNE:
		case DIF_OP_BG:
		case DIF_OP_BGU:
		case DIF_OP_BGE:
		case DIF_OP_BGEU:
		case DIF_OP_BL:
		case DIF_OP_BLU:
		case DIF_OP_BLE:
		case DIF_OP_BLEU:
			if (label >= dp->dtdo_len)
				err += efunc(pc, "invalid branch target %u\n",
					     label);
			if (label <= pc)
				err += efunc(pc, "backward branch to %u\n",
					     label);
			break;
		case DIF_OP_RET:
			if (r1 != 0 || r2 != 0)
				err += efunc(pc, "non-zero reserved bits\n");
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			break;
		case DIF_OP_NOP:
		case DIF_OP_POPTS:
		case DIF_OP_FLUSHTS:
			if (r1 != 0 || r2 != 0 || rd != 0)
				err += efunc(pc, "non-zero reserved bits\n");
			break;
		case DIF_OP_SETX:
			if (DIF_INSTR_INTEGER(instr) >= dp->dtdo_intlen)
				err += efunc(pc, "invalid integer ref %u\n",
					     DIF_INSTR_INTEGER(instr));
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			if (rd == 0)
				err += efunc(pc, "cannot write to %r0\n");
			break;
		case DIF_OP_SETS:
			if (DIF_INSTR_STRING(instr) >= dp->dtdo_strlen)
				err += efunc(pc, "invalid string ref %u\n",
					     DIF_INSTR_STRING(instr));
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			if (rd == 0)
				err += efunc(pc, "cannot write to %r0\n");
			break;
		case DIF_OP_LDGA:
		case DIF_OP_LDTA:
			if (r1 > DIF_VAR_ARRAY_MAX)
				err += efunc(pc, "invalid array %u\n", r1);
			if (r2 >= nregs)
				err += efunc(pc, "invalid register %u\n", r2);
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			if (rd == 0)
				err += efunc(pc, "cannot write to %r0\n");
			break;
		case DIF_OP_LDGS:
		case DIF_OP_LDTS:
		case DIF_OP_LDLS:
		case DIF_OP_LDGAA:
		case DIF_OP_LDTAA:
			if (v < DIF_VAR_OTHER_MIN || v > DIF_VAR_OTHER_MAX)
				err += efunc(pc, "invalid variable %u\n", v);
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			if (rd == 0)
				err += efunc(pc, "cannot write to %r0\n");
			break;
		case DIF_OP_STGS:
		case DIF_OP_STTS:
		case DIF_OP_STLS:
		case DIF_OP_STGAA:
		case DIF_OP_STTAA:
			if (v < DIF_VAR_OTHER_UBASE || v > DIF_VAR_OTHER_MAX)
				err += efunc(pc, "invalid variable %u\n", v);
			if (rs >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			break;
		case DIF_OP_CALL:
			if (subr > DIF_SUBR_MAX)
				err += efunc(pc, "invalid subr %u\n", subr);
			if (rd >= nregs)
				err += efunc(pc, "invalid register %u\n", rd);
			if (rd == 0)
				err += efunc(pc, "cannot write to %r0\n");

			if (subr == DIF_SUBR_COPYOUT ||
			    subr == DIF_SUBR_COPYOUTSTR)
				dp->dtdo_destructive = 1;
			break;
		case DIF_OP_PUSHTR:
			if (type != DIF_TYPE_STRING && type != DIF_TYPE_CTF)
				err += efunc(pc, "invalid ref type %u\n", type);
			if (r2 >= nregs)
				err += efunc(pc, "invalid register %u\n", r2);
			if (rs >= nregs)
				err += efunc(pc, "invalid register %u\n", rs);
			break;
		case DIF_OP_PUSHTV:
			if (type != DIF_TYPE_CTF)
				err += efunc(pc, "invalid val type %u\n", type);
			if (r2 >= nregs)
				err += efunc(pc, "invalid register %u\n", r2);
			if (rs >= nregs)
				err += efunc(pc, "invalid register %u\n", rs);
			break;
		default:
			err += efunc(pc, "invalid opcode %u\n",
				     DIF_INSTR_OP(instr));
		}
	}

	if (dp->dtdo_len != 0 &&
	    DIF_INSTR_OP(dp->dtdo_buf[dp->dtdo_len - 1]) != DIF_OP_RET) {
		err += efunc(dp->dtdo_len - 1,
			     "expected 'ret' as last DIF instruction\n");
	}

	if (!(dp->dtdo_rtype.dtdt_flags & DIF_TF_BYREF)) {
		/*
		 * If we're not returning by reference, the size must be either
		 * 0 or the size of one of the base types.
		 */
		switch (dp->dtdo_rtype.dtdt_size) {
		case 0:
		case sizeof (uint8_t):
		case sizeof (uint16_t):
		case sizeof (uint32_t):
		case sizeof (uint64_t):
			break;

		default:
			err += efunc(dp->dtdo_len - 1, "bad return size\n");
		}
	}

	for (i = 0; i < dp->dtdo_varlen && err == 0; i++) {
		dtrace_difv_t		*v = &dp->dtdo_vartab[i],
					*existing = NULL;
		dtrace_diftype_t	*vt, *et;
		uint_t			id, ndx;

		if (v->dtdv_scope != DIFV_SCOPE_GLOBAL &&
		    v->dtdv_scope != DIFV_SCOPE_THREAD &&
		    v->dtdv_scope != DIFV_SCOPE_LOCAL) {
			err += efunc(i, "unrecognized variable scope %d\n",
				     v->dtdv_scope);
			break;
		}

		if (v->dtdv_kind != DIFV_KIND_ARRAY &&
		    v->dtdv_kind != DIFV_KIND_SCALAR) {
			err += efunc(i, "unrecognized variable type %d\n",
				     v->dtdv_kind);
			break;
		}

		if ((id = v->dtdv_id) > DIF_VARIABLE_MAX) {
			err += efunc(i, "%d exceeds variable id limit\n", id);
			break;
		}

		if (id < DIF_VAR_OTHER_UBASE)
			continue;

		/*
		 * For user-defined variables, we need to check that this
		 * definition is identical to any previous definition that we
		 * encountered.
		 */
		ndx = id - DIF_VAR_OTHER_UBASE;

		switch (v->dtdv_scope) {
		case DIFV_SCOPE_GLOBAL:
			if (ndx < vstate->dtvs_nglobals) {
				dtrace_statvar_t	*svar;

				if ((svar = vstate->dtvs_globals[ndx]) != NULL)
					existing = &svar->dtsv_var;
			}

			break;

		case DIFV_SCOPE_THREAD:
			if (ndx < vstate->dtvs_ntlocals)
				existing = &vstate->dtvs_tlocals[ndx];
			break;

		case DIFV_SCOPE_LOCAL:
			if (ndx < vstate->dtvs_nlocals) {
				dtrace_statvar_t	*svar;

				if ((svar = vstate->dtvs_locals[ndx]) != NULL)
					existing = &svar->dtsv_var;
			}

			break;
		}

		vt = &v->dtdv_type;

		if (vt->dtdt_flags & DIF_TF_BYREF) {
			if (vt->dtdt_size == 0) {
				err += efunc(i, "zero-sized variable\n");
				break;
			}

			if (v->dtdv_scope == DIFV_SCOPE_GLOBAL &&
			    vt->dtdt_size > dtrace_global_maxsize) {
				err += efunc(i, "oversized by-ref global\n");
				break;
			}
		}

		if (existing == NULL || existing->dtdv_id == 0)
			continue;

		ASSERT(existing->dtdv_id == v->dtdv_id);
		ASSERT(existing->dtdv_scope == v->dtdv_scope);

		if (existing->dtdv_kind != v->dtdv_kind)
			err += efunc(i, "%d changed variable kind\n", id);

		et = &existing->dtdv_type;

		if (vt->dtdt_flags != et->dtdt_flags) {
			err += efunc(i, "%d changed variable type flags\n", id);
			break;
		}

		if (vt->dtdt_size != 0 && vt->dtdt_size != et->dtdt_size) {
			err += efunc(i, "%d changed variable type size\n", id);
			break;
		}
	}

	return err;
}

/*
 * Returns 1 if the expression in the DIF object can be cached on a per-thread
 * basis; 0 if not.
 */
int dtrace_difo_cacheable(dtrace_difo_t *dp)
{
	int	i;

	if (dp == NULL)
		return 0;

	for (i = 0; i < dp->dtdo_varlen; i++) {
		dtrace_difv_t	*v = &dp->dtdo_vartab[i];

		if (v->dtdv_scope != DIFV_SCOPE_GLOBAL)
			continue;

		switch (v->dtdv_id) {
		case DIF_VAR_CURTHREAD:
		case DIF_VAR_PID:
		case DIF_VAR_TID:
		case DIF_VAR_EXECNAME:
		case DIF_VAR_ZONENAME:
			break;

		default:
			return 0;
		}
	}

	/*
	 * This DIF object may be cacheable.  Now we need to look for any
	 * array loading instructions, any memory loading instructions, or
	 * any stores to thread-local variables.
	 */
	for (i = 0; i < dp->dtdo_len; i++) {
		uint_t	op = DIF_INSTR_OP(dp->dtdo_buf[i]);

		if ((op >= DIF_OP_LDSB && op <= DIF_OP_LDX) ||
		    (op >= DIF_OP_ULDSB && op <= DIF_OP_ULDX) ||
		    (op >= DIF_OP_RLDSB && op <= DIF_OP_RLDX) ||
		    op == DIF_OP_LDGA || op == DIF_OP_STTS)
			return 0;
	}

	return 1;
}

/*
 * This routine calculates the dynamic variable chunksize for a given DIF
 * object.  The calculation is not fool-proof, and can probably be tricked by
 * malicious DIF -- but it works for all compiler-generated DIF.  Because this
 * calculation is likely imperfect, dtrace_dynvar() is able to gracefully fail
 * if a dynamic variable size exceeds the chunksize.
 */
static void dtrace_difo_chunksize(dtrace_difo_t *dp, dtrace_vstate_t *vstate)
{
	uint64_t		sval;
	dtrace_key_t		tupregs[DIF_DTR_NREGS + 2]; /* + thread + id */
	const dif_instr_t	*text = dp->dtdo_buf;
	uint_t			pc, srd = 0;
	uint_t			ttop = 0;
	size_t			size, ksize;
	uint_t			id, i;

	for (pc = 0; pc < dp->dtdo_len; pc++) {
		dif_instr_t	instr = text[pc];
		uint_t		op = DIF_INSTR_OP(instr);
		uint_t		rd = DIF_INSTR_RD(instr);
		uint_t		r1 = DIF_INSTR_R1(instr);
		uint_t		nkeys = 0;
		uchar_t		scope;
		dtrace_key_t	*key = tupregs;

		switch (op) {
		case DIF_OP_SETX:
			sval = dp->dtdo_inttab[DIF_INSTR_INTEGER(instr)];
			srd = rd;
			continue;

		case DIF_OP_STTS:
			key = &tupregs[DIF_DTR_NREGS];
			key[0].dttk_size = 0;
			key[1].dttk_size = 0;
			nkeys = 2;
			scope = DIFV_SCOPE_THREAD;
			break;

		case DIF_OP_STGAA:
		case DIF_OP_STTAA:
			nkeys = ttop;

			if (DIF_INSTR_OP(instr) == DIF_OP_STTAA)
				key[nkeys++].dttk_size = 0;

			key[nkeys++].dttk_size = 0;

			if (op == DIF_OP_STTAA)
				scope = DIFV_SCOPE_THREAD;
			else
				scope = DIFV_SCOPE_GLOBAL;

			break;

		case DIF_OP_PUSHTR:
			if (ttop == DIF_DTR_NREGS)
				return;

			/*
			 * If the register for the size of the "pushtr" is %r0
			 * (or the value is 0) and the type is a string, we'll
			 * use the system-wide default string size.
			 */
			if ((srd == 0 || sval == 0) && r1 == DIF_TYPE_STRING)
				tupregs[ttop++].dttk_size =
						dtrace_strsize_default;
			else {
				if (srd == 0)
					return;

				tupregs[ttop++].dttk_size = sval;
			}

			break;

		case DIF_OP_PUSHTV:
			if (ttop == DIF_DTR_NREGS)
				return;

			tupregs[ttop++].dttk_size = 0;
			break;

		case DIF_OP_FLUSHTS:
			ttop = 0;
			break;

		case DIF_OP_POPTS:
			if (ttop != 0)
				ttop--;
			break;
		}

		sval = 0;
		srd = 0;

		if (nkeys == 0)
			continue;

		/*
		 * We have a dynamic variable allocation; calculate its size.
		 */
		for (ksize = 0, i = 0; i < nkeys; i++)
			ksize += P2ROUNDUP(key[i].dttk_size, sizeof (uint64_t));

		size = sizeof (dtrace_dynvar_t);
		size += sizeof (dtrace_key_t) * (nkeys - 1);
		size += ksize;

		/*
		 * Now we need to determine the size of the stored data.
		*/
		id = DIF_INSTR_VAR(instr);

		for (i = 0; i < dp->dtdo_varlen; i++) {
			dtrace_difv_t	*v = &dp->dtdo_vartab[i];

			if (v->dtdv_id == id && v->dtdv_scope == scope) {
				size += v->dtdv_type.dtdt_size;
				break;
			}
		}

		if (i == dp->dtdo_varlen)
			return;

		/*
		 * We have the size.  If this is larger than the chunk size
		 * for our dynamic variable state, reset the chunk size.
		 */
		size = P2ROUNDUP(size, sizeof (uint64_t));

		if (size > vstate->dtvs_dynvars.dtds_chunksize)
			vstate->dtvs_dynvars.dtds_chunksize = size;
	}
}

void dtrace_difo_hold(dtrace_difo_t *dp)
{
	int	i;

	dp->dtdo_refcnt++;
	ASSERT(dp->dtdo_refcnt != 0);

	for (i = 0; i < dp->dtdo_varlen; i++) {
		dtrace_difv_t	*v = &dp->dtdo_vartab[i];

		if (v->dtdv_id != DIF_VAR_VTIMESTAMP)
			continue;

		if (dtrace_vtime_references++ == 0)
			dtrace_vtime_enable();
	}
}

void dtrace_difo_init(dtrace_difo_t *dp, dtrace_vstate_t *vstate)
{
	int	i, oldsvars, osz, nsz, otlocals, ntlocals;
	uint_t	id;

	ASSERT(mutex_is_locked(&dtrace_lock));
	ASSERT(dp->dtdo_buf != NULL && dp->dtdo_len != 0);

	for (i = 0; i < dp->dtdo_varlen; i++) {
		dtrace_difv_t		*v = &dp->dtdo_vartab[i];
		dtrace_statvar_t	*svar, ***svarp;
		size_t			dsize = 0;
		uint8_t			scope = v->dtdv_scope;
		int			*np;

		if ((id = v->dtdv_id) < DIF_VAR_OTHER_UBASE)
			continue;

		id -= DIF_VAR_OTHER_UBASE;

		switch (scope) {
		case DIFV_SCOPE_THREAD:
			while (id >= (otlocals = vstate->dtvs_ntlocals)) {
				dtrace_difv_t	*tlocals;

				if ((ntlocals = (otlocals << 1)) == 0)
					ntlocals = 1;

				osz = otlocals * sizeof (dtrace_difv_t);
				nsz = ntlocals * sizeof (dtrace_difv_t);

				tlocals = kzalloc(nsz, GFP_KERNEL);

				if (osz != 0) {
					memcpy(tlocals, vstate->dtvs_tlocals,
					       osz);
					kfree(vstate->dtvs_tlocals);
				}

				vstate->dtvs_tlocals = tlocals;
				vstate->dtvs_ntlocals = ntlocals;
			}

			vstate->dtvs_tlocals[id] = *v;
			continue;

		case DIFV_SCOPE_LOCAL:
			np = &vstate->dtvs_nlocals;
			svarp = &vstate->dtvs_locals;

			if (v->dtdv_type.dtdt_flags & DIF_TF_BYREF)
				dsize = NR_CPUS *
					(v->dtdv_type.dtdt_size +
					 sizeof (uint64_t));
			else
				dsize = NR_CPUS * sizeof (uint64_t);

			break;

		case DIFV_SCOPE_GLOBAL:
			np = &vstate->dtvs_nglobals;
			svarp = &vstate->dtvs_globals;

			if (v->dtdv_type.dtdt_flags & DIF_TF_BYREF)
				dsize = v->dtdv_type.dtdt_size +
					sizeof (uint64_t);

			break;

		default:
			ASSERT(0);
			continue; /* not reached */
		}

		while (id >= (oldsvars = *np)) {
			dtrace_statvar_t	**statics;
			int			newsvars, oldsize, newsize;

			if ((newsvars = (oldsvars << 1)) == 0)
				newsvars = 1;

			oldsize = oldsvars * sizeof (dtrace_statvar_t *);
			newsize = newsvars * sizeof (dtrace_statvar_t *);

			statics = kzalloc(newsize, GFP_KERNEL);

			if (oldsize != 0) {
				memcpy(statics, *svarp, oldsize);
				kfree(*svarp);
			}

			*svarp = statics;
			*np = newsvars;
		}

		if ((svar = (*svarp)[id]) == NULL) {
			svar = kzalloc(sizeof (dtrace_statvar_t), GFP_KERNEL);
			svar->dtsv_var = *v;

			if ((svar->dtsv_size = dsize) != 0) {
				svar->dtsv_data =
					(uint64_t)(uintptr_t)kzalloc(
							dsize, GFP_KERNEL);
			}

			(*svarp)[id] = svar;
		}

		svar->dtsv_refcnt++;
	}

	dtrace_difo_chunksize(dp, vstate);
	dtrace_difo_hold(dp);
}

void dtrace_difo_destroy(dtrace_difo_t *dp, dtrace_vstate_t *vstate)
{
	int	i;

	ASSERT(dp->dtdo_refcnt == 0);

	for (i = 0; i < dp->dtdo_varlen; i++) {
		dtrace_difv_t		*v = &dp->dtdo_vartab[i];
		dtrace_statvar_t	*svar, **svarp;
		uint_t			id;
		uint8_t			scope = v->dtdv_scope;
		int			*np;

		switch (scope) {
		case DIFV_SCOPE_THREAD:
			continue;

		case DIFV_SCOPE_LOCAL:
			np = &vstate->dtvs_nlocals;
			svarp = vstate->dtvs_locals;
			break;

		case DIFV_SCOPE_GLOBAL:
			np = &vstate->dtvs_nglobals;
			svarp = vstate->dtvs_globals;
			break;

		default:
			BUG();
		}

		if ((id = v->dtdv_id) < DIF_VAR_OTHER_UBASE)
			continue;

		id -= DIF_VAR_OTHER_UBASE;
		ASSERT(id < *np);

		svar = svarp[id];
		ASSERT(svar != NULL);
		ASSERT(svar->dtsv_refcnt > 0);

		if (--svar->dtsv_refcnt > 0)
			continue;

		if (svar->dtsv_size != 0) {
			ASSERT((void *)(uintptr_t)svar->dtsv_data != NULL);
			kfree((void *)(uintptr_t)svar->dtsv_data);
		}

		kfree(svar);
		svarp[id] = NULL;
	}

	kfree(dp->dtdo_buf);
        kfree(dp->dtdo_inttab);
        kfree(dp->dtdo_strtab);
        kfree(dp->dtdo_vartab);
        kfree(dp);
}

void dtrace_difo_release(dtrace_difo_t *dp, dtrace_vstate_t *vstate)
{
	int	i;

	ASSERT(mutex_is_locked(&dtrace_lock));
	ASSERT(dp->dtdo_refcnt != 0);

	for (i = 0; i < dp->dtdo_varlen; i++) {
		dtrace_difv_t *v = &dp->dtdo_vartab[i];

		if (v->dtdv_id != DIF_VAR_VTIMESTAMP)
			continue;

		ASSERT(dtrace_vtime_references > 0);

		if (--dtrace_vtime_references == 0)
			dtrace_vtime_disable();
	}

	if (--dp->dtdo_refcnt == 0)
		dtrace_difo_destroy(dp, vstate);
}

/*
 * The key for a thread-local variable consists of the lower 63 bits of the
 * task pid, prefixed by a bit indicating whether an interrupt is active (1) or
 * not (0).
 * We add DIF_VARIABLE_MAX to the pid to assure that the thread key is never
 * equal to a variable identifier.  This is necessary (but not sufficient) to
 * assure that global associative arrays never collide with thread-local
 * variables.  To guarantee that they cannot collide, we must also define the
 * order for keying dynamic variables.  That order is:
 *
 *   [ key0 ] ... [ keyn ] [ variable-key ] [ tls-key ]
 *
 * Because the variable-key and the tls-key are in orthogonal spaces, there is
 * no way for a global variable key signature to match a thread-local key
 * signature.
 */
#define DTRACE_TLS_THRKEY(where)					\
	{								\
		uint_t	intr = in_interrupt() ? 1 : 0;			\
									\
		(where) = ((current->pid + DIF_VARIABLE_MAX) &		\
			   (((uint64_t)1 << 63) - 1)) |			\
			  ((uint64_t)intr << 63);			\
	}
#define DTRACE_ANCHORED(probe)	((probe)->dtpr_func[0] != '\0')

#define DTRACE_INRANGE(testaddr, testsz, baseaddr, basesz) \
	((testaddr) - (baseaddr) < (basesz) && \
	 (testaddr) + (testsz) - (baseaddr) <= (basesz) && \
	 (testaddr) + (testsz) >= (testaddr))

#ifndef FIXME
# define DTRACE_ALIGNCHECK(addr, size, flags)
#endif

#define DTRACE_LOADFUNC(bits)						\
	uint##bits##_t dtrace_load##bits(uintptr_t addr)		\
	{								\
		size_t			size = bits / NBBY;		\
		uint##bits##_t		rval;				\
		int			i;				\
		int			cpu = smp_processor_id();	\
		volatile uint16_t	*flags = (volatile uint16_t *)	\
			    &cpu_core[cpu].cpuc_dtrace_flags;		\
									\
		DTRACE_ALIGNCHECK(addr, size, flags);			\
									\
		for (i = 0; i < dtrace_toxranges; i++) {		\
			if (addr >= dtrace_toxrange[i].dtt_limit)	\
				continue;				\
									\
			if (addr + size <= dtrace_toxrange[i].dtt_base)	\
				continue;				\
								\
			/*						\
			 * This address falls within a toxic region.	\
			 */						\
			*flags |= CPU_DTRACE_BADADDR;			\
			cpu_core[cpu].cpuc_dtrace_illval = addr;	\
			return 0;					\
		}							\
									\
		*flags |= CPU_DTRACE_NOFAULT;				\
		rval = *((volatile uint##bits##_t *)addr);		\
		*flags &= ~CPU_DTRACE_NOFAULT;				\
									\
		return !(*flags & CPU_DTRACE_FAULT) ? rval : 0;		\
	}

#ifdef CONFIG_64BIT
# define dtrace_loadptr	dtrace_load64
#else
# define dtrace_loadptr	dtrace_load32
#endif

DTRACE_LOADFUNC(8)
DTRACE_LOADFUNC(16)
DTRACE_LOADFUNC(32)
DTRACE_LOADFUNC(64)

static int dtrace_canstore_statvar(uint64_t addr, size_t sz,
				   dtrace_statvar_t **svars, int nsvars)
{
	int i;

	for (i = 0; i < nsvars; i++) {
		dtrace_statvar_t	*svar = svars[i];

		if (svar == NULL || svar->dtsv_size == 0)
			continue;

		if (DTRACE_INRANGE(addr, sz, svar->dtsv_data, svar->dtsv_size))
			return 1;
	}

	return 0;
}

/*
 * Check to see if the address is within a memory region to which a store may
 * be issued.  This includes the DTrace scratch areas, and any DTrace variable
 * region.  The caller of dtrace_canstore() is responsible for performing any
 * alignment checks that are needed before stores are actually executed.
 */
static int dtrace_canstore(uint64_t addr, size_t sz, dtrace_mstate_t *mstate,
			   dtrace_vstate_t *vstate)
{
	/*
	 * First, check to see if the address is in scratch space...
	 */
	if (DTRACE_INRANGE(addr, sz, mstate->dtms_scratch_base,
			   mstate->dtms_scratch_size))
		return 1;

	/*
	 * Now check to see if it's a dynamic variable.  This check will pick
	 * up both thread-local variables and any global dynamically-allocated
	 * variables.
	 */
	if (DTRACE_INRANGE(addr, sz, (uintptr_t)vstate->dtvs_dynvars.dtds_base,
			   vstate->dtvs_dynvars.dtds_size)) {
		dtrace_dstate_t	*dstate = &vstate->dtvs_dynvars;
		uintptr_t	base = (uintptr_t)dstate->dtds_base +
				       (dstate->dtds_hashsize *
					sizeof (dtrace_dynhash_t));
		uintptr_t	chunkoffs;

		/*
		 * Before we assume that we can store here, we need to make
		 * sure that it isn't in our metadata -- storing to our
		 * dynamic variable metadata would corrupt our state.  For
		 * the range to not include any dynamic variable metadata,
		 * it must:
		 *
		 *      (1) Start above the hash table that is at the base of
		 *      the dynamic variable space
		 *
		 *      (2) Have a starting chunk offset that is beyond the
		 *      dtrace_dynvar_t that is at the base of every chunk
		 *
		 *      (3) Not span a chunk boundary
		 */
		if (addr < base)
			return 0;

		chunkoffs = (addr - base) % dstate->dtds_chunksize;

		if (chunkoffs < sizeof (dtrace_dynvar_t))
			return 0;

		if (chunkoffs + sz > dstate->dtds_chunksize)
			return 0;

		return 1;
	}

	/*
	 * Finally, check the static local and global variables.  These checks
	 * take the longest, so we perform them last.
	 */
	if (dtrace_canstore_statvar(addr, sz, vstate->dtvs_locals,
				    vstate->dtvs_nlocals))
		return 1;

	if (dtrace_canstore_statvar(addr, sz, vstate->dtvs_globals,
				    vstate->dtvs_nglobals))
		return 1;

	return 0;
}

/*
 * Convenience routine to check to see if the address is within a memory
 * region in which a load may be issued given the user's privilege level;
 * if not, it sets the appropriate error flags and loads 'addr' into the
 * illegal value slot.
 *
 * DTrace subroutines (DIF_SUBR_*) should use this helper to implement
 * appropriate memory access protection.
 */
static int
dtrace_canload(uint64_t addr, size_t sz, dtrace_mstate_t *mstate,
    dtrace_vstate_t *vstate)
{
	int			cpu = smp_processor_id();
	volatile uintptr_t	*illval = &cpu_core[cpu].cpuc_dtrace_illval;

	/*
	 * If we hold the privilege to read from kernel memory, then
	 * everything is readable.
	 */
	if ((mstate->dtms_access & DTRACE_ACCESS_KERNEL) != 0)
		return 1;

	/*
	 * You can obviously read that which you can store.
	 */
	if (dtrace_canstore(addr, sz, mstate, vstate))
		return 1;

	/*
	 * We're allowed to read from our own string table.
	 */
	if (DTRACE_INRANGE(addr, sz, (uintptr_t)mstate->dtms_difo->dtdo_strtab,
			   mstate->dtms_difo->dtdo_strlen))
		return 1;

	DTRACE_CPUFLAG_SET(CPU_DTRACE_KPRIV);
	*illval = addr;

	return 0;
}

/*
 * Convenience routine to check to see if a given string is within a memory
 * region in which a load may be issued given the user's privilege level;
 * this exists so that we don't need to issue unnecessary dtrace_strlen()
 * calls in the event that the user has all privileges.
 */
static int
dtrace_strcanload(uint64_t addr, size_t sz, dtrace_mstate_t *mstate,
    dtrace_vstate_t *vstate)
{
	size_t	strsz;

	/*
	 * If we hold the privilege to read from kernel memory, then
	 * everything is readable.
	 */
	if ((mstate->dtms_access & DTRACE_ACCESS_KERNEL) != 0)
		return 1;

	strsz = 1 + dtrace_strlen((char *)(uintptr_t)addr, sz);
	if (dtrace_canload(addr, strsz, mstate, vstate))
		return 1;

	return 0;
}

/*
 * Convenience routine to check to see if a given variable is within a memory
 * region in which a load may be issued given the user's privilege level.
 */
static int dtrace_vcanload(void *src, dtrace_diftype_t *type,
			   dtrace_mstate_t *mstate, dtrace_vstate_t *vstate)
{
	size_t	sz;

	ASSERT(type->dtdt_flags & DIF_TF_BYREF);

	/*
	 * If we hold the privilege to read from kernel memory, then
	 * everything is readable.
	 */
	if ((mstate->dtms_access & DTRACE_ACCESS_KERNEL) != 0)
		return 1;

	if (type->dtdt_kind == DIF_TYPE_STRING)
		sz = dtrace_strlen(
			src,
			vstate->dtvs_state->dts_options[DTRACEOPT_STRSIZE]
		     ) + 1;
	else
		sz = type->dtdt_size;

	return dtrace_canload((uintptr_t)src, sz, mstate, vstate);
}

/*
 * Copy src to dst using safe memory accesses.  The src is assumed to be unsafe
 * memory specified by the DIF program.  The dst is assumed to be safe memory
 * that we can store to directly because it is managed by DTrace.  As with
 * standard bcopy, overlapping copies are handled properly.
 */
static void dtrace_bcopy(const void *src, void *dst, size_t len)
{
	if (len != 0) {
		uint8_t		*s1 = dst;
		const uint8_t	*s2 = src;

		if (s1 <= s2) {
			do {
				*s1++ = dtrace_load8((uintptr_t)s2++);
			} while (--len != 0);
		} else {
			s2 += len;
			s1 += len;

			do {
				*--s1 = dtrace_load8((uintptr_t)--s2);
			} while (--len != 0);
		}
	}
}

/*
 * Copy src to dst using safe memory accesses, up to either the specified
 * length, or the point that a nul byte is encountered.  The src is assumed to
 * be unsafe memory specified by the DIF program.  The dst is assumed to be
 * safe memory that we can store to directly because it is managed by DTrace.
 * Unlike dtrace_bcopy(), overlapping regions are not handled.
 */
static void dtrace_strcpy(const void *src, void *dst, size_t len)
{
	if (len != 0) {
		uint8_t		*s1 = dst, c;
		const uint8_t	*s2 = src;

		do {
			*s1++ = c = dtrace_load8((uintptr_t)s2++);
		} while (--len != 0 && c != '\0');
	}
}
/*
 * Copy src to dst, deriving the size and type from the specified (BYREF)
 * variable type.  The src is assumed to be unsafe memory specified by the DIF
 * program.  The dst is assumed to be DTrace variable memory that is of the
 * specified type; we assume that we can store to directly.
 */
static void dtrace_vcopy(void *src, void *dst, dtrace_diftype_t *type)
{
	ASSERT(type->dtdt_flags & DIF_TF_BYREF);

	if (type->dtdt_kind == DIF_TYPE_STRING)
		dtrace_strcpy(src, dst, type->dtdt_size);
	else
		dtrace_bcopy(src, dst, type->dtdt_size);
}

/*
 * Return a string.  In the event that the user lacks the privilege to access
 * arbitrary kernel memory, we copy the string out to scratch memory so that we
 * don't fail access checking.
 *
 * dtrace_dif_variable() uses this routine as a helper for various
 * builtin values such as 'execname' and 'probefunc.'
 */
static uintptr_t dtrace_dif_varstr(uintptr_t addr, dtrace_state_t *state,
				   dtrace_mstate_t *mstate)
{
	uint64_t	size = state->dts_options[DTRACEOPT_STRSIZE];
	uintptr_t	ret;
	size_t		strsz;

	/*
	 * The easy case: this probe is allowed to read all of memory, so
	 * we can just return this as a vanilla pointer.
	 */
	if ((mstate->dtms_access & DTRACE_ACCESS_KERNEL) != 0)
		return addr;

	/*
	 * This is the tougher case: we copy the string in question from
	 * kernel memory into scratch memory and return it that way: this
	 * ensures that we won't trip up when access checking tests the
	 * BYREF return value.
	 */
	strsz = dtrace_strlen((char *)addr, size) + 1;

	if (mstate->dtms_scratch_ptr + strsz >
	    mstate->dtms_scratch_base + mstate->dtms_scratch_size) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_NOSCRATCH);
		return (uintptr_t)NULL;
	}

	dtrace_strcpy((const void *)addr, (void *)mstate->dtms_scratch_ptr,
		      strsz);
	ret = mstate->dtms_scratch_ptr;
	mstate->dtms_scratch_ptr += strsz;

	return ret;
}

/*
 * This function implements the DIF emulator's variable lookups.  The emulator
 * passes a reserved variable identifier and optional built-in array index.
 */
static uint64_t dtrace_dif_variable(dtrace_mstate_t *mstate,
				    dtrace_state_t *state, uint64_t v,
				    uint64_t ndx)
{
	/*
	 * If we're accessing one of the uncached arguments, we'll turn this
	 * into a reference in the args array.
	 */
	if (v >= DIF_VAR_ARG0 && v <= DIF_VAR_ARG9) {
		ndx = v - DIF_VAR_ARG0;
		v = DIF_VAR_ARGS;
	}

	switch (v) {
	case DIF_VAR_ARGS:
		ASSERT(mstate->dtms_present & DTRACE_MSTATE_ARGS);

		if (ndx >=
		    sizeof (mstate->dtms_arg) / sizeof (mstate->dtms_arg[0])) {
			int			aframes =
					mstate->dtms_probe->dtpr_aframes + 2;
			dtrace_provider_t	*pv;
			uint64_t		val;

			pv = mstate->dtms_probe->dtpr_provider;
			if (pv->dtpv_pops.dtps_getargval != NULL)
				val = pv->dtpv_pops.dtps_getargval(
					pv->dtpv_arg,
					mstate->dtms_probe->dtpr_id,
					mstate->dtms_probe->dtpr_arg,
					ndx, aframes);
			else
				val = dtrace_getarg(ndx, aframes);

			/*
			 * This is regrettably required to keep the compiler
			 * from tail-optimizing the call to dtrace_getarg().
			 * The condition always evaluates to true, but the
			 * compiler has no way of figuring that out a priori.
			 * (None of this would be necessary if the compiler
			 * could be relied upon to _always_ tail-optimize
			 * the call to dtrace_getarg() -- but it can't.)
			 */
			if (mstate->dtms_probe != NULL)
				return val;

			ASSERT(0);
		}

		return mstate->dtms_arg[ndx];

	case DIF_VAR_UREGS: {
		struct pt_regs	*regs = task_pt_regs(current);

		if (!dtrace_priv_proc(state))
			return 0;

		return dtrace_getreg(regs, ndx);
	}

	case DIF_VAR_CURTHREAD:
		if (!dtrace_priv_kernel(state))
			return 0;

		return (uint64_t)(uintptr_t)current;

	case DIF_VAR_TIMESTAMP:
		if (!(mstate->dtms_present & DTRACE_MSTATE_TIMESTAMP)) {
			mstate->dtms_timestamp = dtrace_gethrtime();
			mstate->dtms_present |= DTRACE_MSTATE_TIMESTAMP;
		}

		return mstate->dtms_timestamp;

	case DIF_VAR_VTIMESTAMP:
		ASSERT(dtrace_vtime_references != 0);

		return current->dtrace_vtime;

	case DIF_VAR_WALLTIMESTAMP:
		if (!(mstate->dtms_present & DTRACE_MSTATE_WALLTIMESTAMP)) {
			mstate->dtms_walltimestamp = dtrace_gethrestime();
			mstate->dtms_present |= DTRACE_MSTATE_WALLTIMESTAMP;
		}

		return mstate->dtms_walltimestamp;

	case DIF_VAR_IPL:
		if (!dtrace_priv_kernel(state))
			return 0;

		if (!(mstate->dtms_present & DTRACE_MSTATE_IPL)) {
			mstate->dtms_ipl = dtrace_getipl();
			mstate->dtms_present |= DTRACE_MSTATE_IPL;
		}

		return mstate->dtms_ipl;

	case DIF_VAR_EPID:
		ASSERT(mstate->dtms_present & DTRACE_MSTATE_EPID);
		ASSERT(mstate->dtms_present & DTRACE_MSTATE_EPID);

		return mstate->dtms_epid;

	case DIF_VAR_ID:
		ASSERT(mstate->dtms_present & DTRACE_MSTATE_PROBE);
		return mstate->dtms_probe->dtpr_id;

	case DIF_VAR_STACKDEPTH:
		if (!dtrace_priv_kernel(state))
			return 0;
		if (!(mstate->dtms_present & DTRACE_MSTATE_STACKDEPTH)) {
			int	aframes = mstate->dtms_probe->dtpr_aframes + 2;

			mstate->dtms_stackdepth = dtrace_getstackdepth(aframes);
			mstate->dtms_present |= DTRACE_MSTATE_STACKDEPTH;
		}

		return mstate->dtms_stackdepth;

	case DIF_VAR_USTACKDEPTH:
		if (!dtrace_priv_proc(state))
			return 0;

		if (!(mstate->dtms_present & DTRACE_MSTATE_USTACKDEPTH)) {
			/*
			 * See comment in DIF_VAR_PID.
			 */
			if (DTRACE_ANCHORED(mstate->dtms_probe) &&
			    in_interrupt())
				mstate->dtms_ustackdepth = 0;
			else {
				DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
				mstate->dtms_ustackdepth =
						dtrace_getustackdepth();
				DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT);
			}

			mstate->dtms_present |= DTRACE_MSTATE_USTACKDEPTH;
		}

		return mstate->dtms_ustackdepth;

	case DIF_VAR_CALLER:
		if (!dtrace_priv_kernel(state))
			return 0;

		if (!(mstate->dtms_present & DTRACE_MSTATE_CALLER)) {
			int	aframes = mstate->dtms_probe->dtpr_aframes + 2;

			if (!DTRACE_ANCHORED(mstate->dtms_probe)) {
				/*
				 * If this is an unanchored probe, we are
				 * required to go through the slow path:
				 * dtrace_caller() only guarantees correct
				 * results for anchored probes.
				 */
				pc_t	caller[2];

				dtrace_getpcstack(caller, 2, aframes,
					(uint32_t *)(uintptr_t)
							mstate->dtms_arg[0]);
				mstate->dtms_caller = caller[1];
			} else if ((mstate->dtms_caller =
					dtrace_caller(aframes)) == -1) {
				/*
				 * We have failed to do this the quick way;
				 * we must resort to the slower approach of
				 * calling dtrace_getpcstack().
				 */
				pc_t	caller;

				dtrace_getpcstack(&caller, 1, aframes, NULL);
				mstate->dtms_caller = caller;
			}

			mstate->dtms_present |= DTRACE_MSTATE_CALLER;
		}

		return mstate->dtms_caller;

	case DIF_VAR_UCALLER:
		if (!dtrace_priv_proc(state))
			return 0;

		if (!(mstate->dtms_present & DTRACE_MSTATE_UCALLER)) {
			uint64_t	ustack[3];

			/*
			 * dtrace_getupcstack() fills in the first uint64_t
			 * with the current PID.  The second uint64_t will
			 * be the program counter at user-level.  The third
			 * uint64_t will contain the caller, which is what
			 * we're after.
			 */
			ustack[2] = 0;
			DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
			dtrace_getupcstack(ustack, 3);
			DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT);
			mstate->dtms_ucaller = ustack[2];
			mstate->dtms_present |= DTRACE_MSTATE_UCALLER;
		}

		return mstate->dtms_ucaller;

	case DIF_VAR_PROBEPROV:
		ASSERT(mstate->dtms_present & DTRACE_MSTATE_PROBE);

		return dtrace_dif_varstr(
			(uintptr_t)mstate->dtms_probe->dtpr_provider->dtpv_name,
			state, mstate);

	case DIF_VAR_PROBEMOD:
		ASSERT(mstate->dtms_present & DTRACE_MSTATE_PROBE);
		return dtrace_dif_varstr(
			(uintptr_t)mstate->dtms_probe->dtpr_mod, state,
			mstate);

	case DIF_VAR_PROBEFUNC:
		ASSERT(mstate->dtms_present & DTRACE_MSTATE_PROBE);

		return dtrace_dif_varstr(
			(uintptr_t)mstate->dtms_probe->dtpr_func, state,
			mstate);

	case DIF_VAR_PROBENAME:
		ASSERT(mstate->dtms_present & DTRACE_MSTATE_PROBE);

		return dtrace_dif_varstr(
			(uintptr_t)mstate->dtms_probe->dtpr_name, state,
			mstate);

	case DIF_VAR_PID:
		if (!dtrace_priv_proc(state))
			return 0;

		/*
		 * Note that we are assuming that an unanchored probe is
		 * always due to a high-level interrupt.  (And we're assuming
		 * that there is only a single high level interrupt.)
		 */
		if (DTRACE_ANCHORED(mstate->dtms_probe) && in_interrupt())
			return init_task.pid;

		/*
		 * It is always safe to dereference current, it always points
		 * to a valid task_struct.
		 */
		return (uint64_t)current->pid;

	case DIF_VAR_PPID:
		if (!dtrace_priv_proc(state))
			return 0;

		/*
		 * See comment in DIF_VAR_PID.
		 */
		if (DTRACE_ANCHORED(mstate->dtms_probe) && in_interrupt())
			return init_task.real_parent->pid;

		/*
		 * It is always safe to dereference current, it always points
		 * to a valid task_struct.
		 *
		 * Additionally, it is safe to dereference one's parent, since
		 * it is never NULL after process birth.
		 */
		return (uint64_t)current->real_parent->pid;

	case DIF_VAR_TID:
		/*
		 * See comment in DIF_VAR_PID.
		 */
		if (DTRACE_ANCHORED(mstate->dtms_probe) && in_interrupt())
			return init_task.pid;

		return (uint64_t)current->pid;

	case DIF_VAR_EXECNAME:
		if (!dtrace_priv_proc(state))
			return 0;

		/*
		 * See comment in DIF_VAR_PID.
		 */
		if (DTRACE_ANCHORED(mstate->dtms_probe) && in_interrupt())
			return (uint64_t)(uintptr_t)init_task.comm;

		/*
		 * It is always safe to dereference current, it always points
		 * to a valid task_struct.
		 */
		return dtrace_dif_varstr((uintptr_t)current->comm, state,
					 mstate);

	case DIF_VAR_ZONENAME:
		return 0;

	case DIF_VAR_UID:
		if (!dtrace_priv_proc(state))
			return 0;

		/*
		 * See comment in DIF_VAR_PID.
		 */
		if (DTRACE_ANCHORED(mstate->dtms_probe) && in_interrupt())
			return (uint64_t)init_task.real_cred->uid;

		/*
		 * It is always safe to dereference current, it always points
		 * to a valid task_struct.
		 *
		 * Additionally, it is safe to dereference one's own process
		 * credential, since this is never NULL after process birth.
		 */
		return (uint64_t)current->real_cred->uid;

	case DIF_VAR_GID:
		if (!dtrace_priv_proc(state))
			return 0;

		/*
		 * See comment in DIF_VAR_PID.
		 */
		if (DTRACE_ANCHORED(mstate->dtms_probe) && in_interrupt())
			return (uint64_t)init_task.real_cred->gid;

		/*
		 * It is always safe to dereference current, it always points
		 * to a valid task_struct.
		 *
		 * Additionally, it is safe to dereference one's own process
		 * credential, since this is never NULL after process birth.
		 */
		return (uint64_t)current->real_cred->gid;

	case DIF_VAR_ERRNO:
		if (!dtrace_priv_proc(state))
			return 0;

		/*
		 * See comment in DIF_VAR_PID.
		 */
		if (DTRACE_ANCHORED(mstate->dtms_probe) && in_interrupt())
			return 0;

		/*
		 * It is always safe to dereference current, it always points
		 * to a valid task_struct.
		 */
		return (uint64_t)current->thread.error_code;

	default:
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return 0;
	}
}

/*
 * Emulate the execution of DTrace IR instructions specified by the given DIF
 * object.  This function is deliberately void fo assertions as all of the
 * necessary checks are handled by a call to dtrace_difo_validate().
 */
uint64_t dtrace_dif_emulate(dtrace_difo_t *difo, dtrace_mstate_t *mstate,
			    dtrace_vstate_t *vstate, dtrace_state_t *state)
{
	const dif_instr_t	*text = difo->dtdo_buf;
	const uint_t		textlen = difo->dtdo_len;
	const char		*strtab = difo->dtdo_strtab;
	const uint64_t		*inttab = difo->dtdo_inttab;

	int			cpu = smp_processor_id();
	uint64_t		rval = 0;
	dtrace_statvar_t	*svar;
	dtrace_dstate_t		*dstate = &vstate->dtvs_dynvars;
	dtrace_difv_t		*v;
	volatile uint16_t	*flags = &cpu_core[cpu].cpuc_dtrace_flags;
	volatile uintptr_t	*illval = &cpu_core[cpu].cpuc_dtrace_illval;

	dtrace_key_t		tupregs[DIF_DTR_NREGS + 2];
						/* +2 for thread and id */
	uint64_t		regs[DIF_DIR_NREGS];
	uint64_t		*tmp;

	uint8_t			cc_n = 0, cc_z = 0, cc_v = 0, cc_c = 0;
	int64_t			cc_r;
	uint_t			pc = 0, id, opc;
	uint8_t			ttop = 0;
	dif_instr_t		instr;
	uint_t			r1, r2, rd;

	/*
	 * We stash the current DIF object into the machine state: we need it
	 * for subsequent access checking.
	 */
	mstate->dtms_difo = difo;

	regs[DIF_REG_R0] = 0;			/* %r0 is fixed at zero */

	while (pc < textlen && !(*flags & CPU_DTRACE_FAULT)) {
		opc = pc;

		instr = text[pc++];
		r1 = DIF_INSTR_R1(instr);
		r2 = DIF_INSTR_R2(instr);
		rd = DIF_INSTR_RD(instr);

		switch (DIF_INSTR_OP(instr)) {
		case DIF_OP_OR:
			regs[rd] = regs[r1] | regs[r2];
			break;
		case DIF_OP_XOR:
			regs[rd] = regs[r1] ^ regs[r2];
			break;
		case DIF_OP_AND:
			regs[rd] = regs[r1] & regs[r2];
			break;
		case DIF_OP_SLL:
			regs[rd] = regs[r1] << regs[r2];
			break;
		case DIF_OP_SRL:
			regs[rd] = regs[r1] >> regs[r2];
			break;
		case DIF_OP_SUB:
			regs[rd] = regs[r1] - regs[r2];
			break;
		case DIF_OP_ADD:
			regs[rd] = regs[r1] + regs[r2];
			break;
		case DIF_OP_MUL:
			regs[rd] = regs[r1] * regs[r2];
			break;
		case DIF_OP_SDIV:
			if (regs[r2] == 0) {
				regs[rd] = 0;
				*flags |= CPU_DTRACE_DIVZERO;
			} else {
				regs[rd] = (int64_t)regs[r1] /
					   (int64_t)regs[r2];
			}
			break;

		case DIF_OP_UDIV:
			if (regs[r2] == 0) {
				regs[rd] = 0;
				*flags |= CPU_DTRACE_DIVZERO;
			} else {
				regs[rd] = regs[r1] / regs[r2];
			}
			break;

		case DIF_OP_SREM:
			if (regs[r2] == 0) {
				regs[rd] = 0;
				*flags |= CPU_DTRACE_DIVZERO;
			} else {
				regs[rd] = (int64_t)regs[r1] %
					   (int64_t)regs[r2];
			}
			break;

		case DIF_OP_UREM:
			if (regs[r2] == 0) {
				regs[rd] = 0;
				*flags |= CPU_DTRACE_DIVZERO;
			} else
				regs[rd] = regs[r1] % regs[r2];
			break;

		case DIF_OP_NOT:
			regs[rd] = ~regs[r1];
			break;
		case DIF_OP_MOV:
			regs[rd] = regs[r1];
			break;
		case DIF_OP_CMP:
			cc_r = regs[r1] - regs[r2];
			cc_n = cc_r < 0;
			cc_z = cc_r == 0;
			cc_v = 0;
			cc_c = regs[r1] < regs[r2];
			break;
		case DIF_OP_TST:
			cc_n = cc_v = cc_c = 0;
			cc_z = regs[r1] == 0;
			break;
		case DIF_OP_BA:
			pc = DIF_INSTR_LABEL(instr);
			break;
		case DIF_OP_BE:
			if (cc_z)
				pc = DIF_INSTR_LABEL(instr);
			break;
		case DIF_OP_BNE:
			if (cc_z == 0)
				pc = DIF_INSTR_LABEL(instr);
			break;
		case DIF_OP_BG:
			if ((cc_z | (cc_n ^ cc_v)) == 0)
				pc = DIF_INSTR_LABEL(instr);
			break;
		case DIF_OP_BGU:
			if ((cc_c | cc_z) == 0)
				pc = DIF_INSTR_LABEL(instr);
			break;
		case DIF_OP_BGE:
			if ((cc_n ^ cc_v) == 0)
				pc = DIF_INSTR_LABEL(instr);
			break;
		case DIF_OP_BGEU:
			if (cc_c == 0)
				pc = DIF_INSTR_LABEL(instr);
			break;
		case DIF_OP_BL:
			if (cc_n ^ cc_v)
				pc = DIF_INSTR_LABEL(instr);
			break;
		case DIF_OP_BLU:
			if (cc_c)
				pc = DIF_INSTR_LABEL(instr);
			break;
		case DIF_OP_BLE:
			if (cc_z | (cc_n ^ cc_v))
				pc = DIF_INSTR_LABEL(instr);
			break;
		case DIF_OP_BLEU:
			if (cc_c | cc_z)
				pc = DIF_INSTR_LABEL(instr);
			break;
		case DIF_OP_RLDSB:
			if (!dtrace_canstore(regs[r1], 1, mstate, vstate)) {
				*flags |= CPU_DTRACE_KPRIV;
				*illval = regs[r1];
				break;
			}
			/*FALLTHROUGH*/
		case DIF_OP_LDSB:
			regs[rd] = (int8_t)dtrace_load8(regs[r1]);
			break;
		case DIF_OP_RLDSH:
			if (!dtrace_canstore(regs[r1], 2, mstate, vstate)) {
				*flags |= CPU_DTRACE_KPRIV;
				*illval = regs[r1];
				break;
			}
			/*FALLTHROUGH*/
		case DIF_OP_LDSH:
			regs[rd] = (int16_t)dtrace_load16(regs[r1]);
			break;
		case DIF_OP_RLDSW:
			if (!dtrace_canstore(regs[r1], 4, mstate, vstate)) {
				*flags |= CPU_DTRACE_KPRIV;
				*illval = regs[r1];
				break;
			}
			/*FALLTHROUGH*/
		case DIF_OP_LDSW:
			regs[rd] = (int32_t)dtrace_load32(regs[r1]);
			break;
		case DIF_OP_RLDUB:
			if (!dtrace_canstore(regs[r1], 1, mstate, vstate)) {
				*flags |= CPU_DTRACE_KPRIV;
				*illval = regs[r1];
				break;
			}
			/*FALLTHROUGH*/
		case DIF_OP_LDUB:
			regs[rd] = dtrace_load8(regs[r1]);
			break;
		case DIF_OP_RLDUH:
			if (!dtrace_canstore(regs[r1], 2, mstate, vstate)) {
				*flags |= CPU_DTRACE_KPRIV;
				*illval = regs[r1];
				break;
			}
			/*FALLTHROUGH*/
		case DIF_OP_LDUH:
			regs[rd] = dtrace_load16(regs[r1]);
			break;
		case DIF_OP_RLDUW:
			if (!dtrace_canstore(regs[r1], 4, mstate, vstate)) {
				*flags |= CPU_DTRACE_KPRIV;
				*illval = regs[r1];
				break;
			}
			/*FALLTHROUGH*/
		case DIF_OP_LDUW:
			regs[rd] = dtrace_load32(regs[r1]);
			break;
		case DIF_OP_RLDX:
			if (!dtrace_canstore(regs[r1], 8, mstate, vstate)) {
				*flags |= CPU_DTRACE_KPRIV;
				*illval = regs[r1];
				break;
			}
			/*FALLTHROUGH*/
		case DIF_OP_LDX:
			regs[rd] = dtrace_load64(regs[r1]);
			break;
		case DIF_OP_ULDSB:
			regs[rd] = (int8_t)dtrace_fuword8(
						(void *)(uintptr_t)regs[r1]);
			break;
		case DIF_OP_ULDSH:
			regs[rd] = (int16_t)dtrace_fuword16(
						(void *)(uintptr_t)regs[r1]);
			break;
		case DIF_OP_ULDSW:
			regs[rd] = (int32_t)dtrace_fuword32(
						(void *)(uintptr_t)regs[r1]);
			break;
		case DIF_OP_ULDUB:
			regs[rd] = dtrace_fuword8((void *)(uintptr_t)regs[r1]);
			break;
		case DIF_OP_ULDUH:
			regs[rd] = dtrace_fuword16(
						(void *)(uintptr_t)regs[r1]);
			break;
		case DIF_OP_ULDUW:
			regs[rd] = dtrace_fuword32(
						(void *)(uintptr_t)regs[r1]);
			break;
		case DIF_OP_ULDX:
			regs[rd] = dtrace_fuword64(
						(void *)(uintptr_t)regs[r1]);
			break;
		case DIF_OP_RET:
			rval = regs[rd];
			pc = textlen;
			break;
		case DIF_OP_NOP:
			break;
		case DIF_OP_SETX:
			regs[rd] = inttab[DIF_INSTR_INTEGER(instr)];
			break;
		case DIF_OP_SETS:
			regs[rd] = (uint64_t)(uintptr_t)
					(strtab + DIF_INSTR_STRING(instr));
			break;
		case DIF_OP_SCMP: {
			size_t		sz = state->dts_options[
							DTRACEOPT_STRSIZE];
			uintptr_t	s1 = regs[r1];
			uintptr_t	s2 = regs[r2];

			if (s1 != (uintptr_t)NULL &&
			    !dtrace_strcanload(s1, sz, mstate, vstate))
				break;
			if (s2 != (uintptr_t)NULL &&
			    !dtrace_strcanload(s2, sz, mstate, vstate))
				break;

			cc_r = dtrace_strncmp((char *)s1, (char *)s2, sz);

			cc_n = cc_r < 0;
			cc_z = cc_r == 0;
			cc_v = cc_c = 0;
			break;
		}
		case DIF_OP_LDGA:
		    regs[rd] = dtrace_dif_variable(mstate, state, r1,
						   regs[r2]);
			break;
		case DIF_OP_LDGS:
			id = DIF_INSTR_VAR(instr);

			if (id >= DIF_VAR_OTHER_UBASE) {
				uintptr_t	a;

				id -= DIF_VAR_OTHER_UBASE;
				svar = vstate->dtvs_globals[id];
				ASSERT(svar != NULL);
				v = &svar->dtsv_var;

				if (!(v->dtdv_type.dtdt_flags & DIF_TF_BYREF)) {
					regs[rd] = svar->dtsv_data;
					break;
				}

				a = (uintptr_t)svar->dtsv_data;

				/*
				 * If the 0th byte is set to UINT8_MAX then
				 * this is to be treated as a reference to a
				 * NULL variable.
				 */
				if (*(uint8_t *)a == UINT8_MAX)
					regs[rd] = 0;
				else
					regs[rd] = a + sizeof (uint64_t);

				break;
			}

			regs[rd] = dtrace_dif_variable(mstate, state, id, 0);
			break;

		case DIF_OP_STGS:
			id = DIF_INSTR_VAR(instr);

			ASSERT(id >= DIF_VAR_OTHER_UBASE);
			id -= DIF_VAR_OTHER_UBASE;

			svar = vstate->dtvs_globals[id];
			ASSERT(svar != NULL);
			v = &svar->dtsv_var;

			if (v->dtdv_type.dtdt_flags & DIF_TF_BYREF) {
				uintptr_t	a = (uintptr_t)svar->dtsv_data;

				ASSERT(a != NULL);
				ASSERT(svar->dtsv_size != 0);

				if (regs[rd] == 0) {
					*(uint8_t *)a = UINT8_MAX;
					break;
				} else {
					*(uint8_t *)a = 0;
					a += sizeof (uint64_t);
				}

				if (!dtrace_vcanload(
					(void *)(uintptr_t)regs[rd],
					&v->dtdv_type, mstate, vstate))
					break;

				dtrace_vcopy((void *)(uintptr_t)regs[rd],
					     (void *)a, &v->dtdv_type);
				break;
			}

			svar->dtsv_data = regs[rd];
			break;

		case DIF_OP_LDTA:
			/*
			 * There are no DTrace built-in thread-local arrays at
			 * present.  This opcode is saved for future work.
			 */
			*flags |= CPU_DTRACE_ILLOP;
			regs[rd] = 0;
			break;

		case DIF_OP_LDLS:
			id = DIF_INSTR_VAR(instr);

			if (id < DIF_VAR_OTHER_UBASE) {
				/*
				 * For now, this has no meaning.
				 */
				regs[rd] = 0;
				break;
			}

			id -= DIF_VAR_OTHER_UBASE;

			ASSERT(id < vstate->dtvs_nlocals);
			ASSERT(vstate->dtvs_locals != NULL);

			svar = vstate->dtvs_locals[id];
			ASSERT(svar != NULL);
			v = &svar->dtsv_var;

			if (v->dtdv_type.dtdt_flags & DIF_TF_BYREF) {
				uintptr_t	a = (uintptr_t)svar->dtsv_data;
				size_t		sz = v->dtdv_type.dtdt_size;

				sz += sizeof (uint64_t);
				ASSERT(svar->dtsv_size == NR_CPUS * sz);
				a += smp_processor_id() * sz;

				if (*(uint8_t *)a == UINT8_MAX) {
					/*
					 * If the 0th byte is set to UINT8_MAX
					 * then this is to be treated as a
					 * reference to a NULL variable.
					 */
					regs[rd] = 0;
				} else
					regs[rd] = a + sizeof (uint64_t);

				break;
			}

			ASSERT(svar->dtsv_size == NR_CPUS * sizeof (uint64_t));
			tmp = (uint64_t *)(uintptr_t)svar->dtsv_data;
			regs[rd] = tmp[smp_processor_id()];
			break;

		case DIF_OP_STLS:
			id = DIF_INSTR_VAR(instr);

			ASSERT(id >= DIF_VAR_OTHER_UBASE);
			id -= DIF_VAR_OTHER_UBASE;
			ASSERT(id < vstate->dtvs_nlocals);

			ASSERT(vstate->dtvs_locals != NULL);
			svar = vstate->dtvs_locals[id];
			ASSERT(svar != NULL);
			v = &svar->dtsv_var;

			if (v->dtdv_type.dtdt_flags & DIF_TF_BYREF) {
				uintptr_t	a = (uintptr_t)svar->dtsv_data;
				size_t		sz = v->dtdv_type.dtdt_size;

				sz += sizeof (uint64_t);
				ASSERT(svar->dtsv_size == NR_CPUS * sz);
				a += smp_processor_id() * sz;

				if (regs[rd] == 0) {
					*(uint8_t *)a = UINT8_MAX;
					break;
				} else {
					*(uint8_t *)a = 0;
					a += sizeof (uint64_t);
				}

				if (!dtrace_vcanload(
						(void *)(uintptr_t)regs[rd],
						&v->dtdv_type, mstate, vstate))
					break;

				dtrace_vcopy((void *)(uintptr_t)regs[rd],
					     (void *)a, &v->dtdv_type);
				break;
			}

			ASSERT(svar->dtsv_size == NR_CPUS * sizeof (uint64_t));
			tmp = (uint64_t *)(uintptr_t)svar->dtsv_data;
			tmp[smp_processor_id()] = regs[rd];
			break;

		case DIF_OP_LDTS: {
			dtrace_dynvar_t	*dvar;
			dtrace_key_t	*key;

			id = DIF_INSTR_VAR(instr);
			ASSERT(id >= DIF_VAR_OTHER_UBASE);
			id -= DIF_VAR_OTHER_UBASE;
			v = &vstate->dtvs_tlocals[id];

			key = &tupregs[DIF_DTR_NREGS];
			key[0].dttk_value = (uint64_t)id;
			key[0].dttk_size = 0;
			DTRACE_TLS_THRKEY(key[1].dttk_value);
			key[1].dttk_size = 0;

			dvar = dtrace_dynvar(dstate, 2, key, sizeof (uint64_t),
					     DTRACE_DYNVAR_NOALLOC, mstate,
					     vstate);

			if (dvar == NULL) {
				regs[rd] = 0;
				break;
			}

			if (v->dtdv_type.dtdt_flags & DIF_TF_BYREF)
				regs[rd] = (uint64_t)(uintptr_t)dvar->dtdv_data;
			else
				regs[rd] = *((uint64_t *)dvar->dtdv_data);

			break;
		}

		case DIF_OP_STTS: {
			dtrace_dynvar_t	*dvar;
			dtrace_key_t	*key;

			id = DIF_INSTR_VAR(instr);
			ASSERT(id >= DIF_VAR_OTHER_UBASE);
			id -= DIF_VAR_OTHER_UBASE;

			key = &tupregs[DIF_DTR_NREGS];
			key[0].dttk_value = (uint64_t)id;
			key[0].dttk_size = 0;
			DTRACE_TLS_THRKEY(key[1].dttk_value);
			key[1].dttk_size = 0;
			v = &vstate->dtvs_tlocals[id];

			dvar = dtrace_dynvar(dstate, 2, key,
				v->dtdv_type.dtdt_size > sizeof (uint64_t)
					?  v->dtdv_type.dtdt_size
					: sizeof (uint64_t),
				regs[rd]
					? DTRACE_DYNVAR_ALLOC
					: DTRACE_DYNVAR_DEALLOC,
				mstate, vstate);

			/*
			 * Given that we're storing to thread-local data,
			 * we need to flush our predicate cache.
			 */
			current->predcache = NULL;

			if (dvar == NULL)
				break;

			if (v->dtdv_type.dtdt_flags & DIF_TF_BYREF) {
				if (!dtrace_vcanload(
						(void *)(uintptr_t)regs[rd],
						&v->dtdv_type, mstate, vstate))
					break;

				dtrace_vcopy((void *)(uintptr_t)regs[rd],
					     dvar->dtdv_data, &v->dtdv_type);
			} else
				*((uint64_t *)dvar->dtdv_data) = regs[rd];

			break;
		}

		case DIF_OP_SRA:
			regs[rd] = (int64_t)regs[r1] >> regs[r2];
			break;

		case DIF_OP_CALL:
			dtrace_dif_subr(DIF_INSTR_SUBR(instr), rd, regs,
					tupregs, ttop, mstate, state);
			break;

		case DIF_OP_PUSHTR:
			if (ttop == DIF_DTR_NREGS) {
				*flags |= CPU_DTRACE_TUPOFLOW;
				break;
			}

			if (r1 == DIF_TYPE_STRING)
				/*
				 * If this is a string type and the size is 0,
				 * we'll use the system-wide default string
				 * size.  Note that we are _not_ looking at
				 * the value of the DTRACEOPT_STRSIZE option;
				 * had this been set, we would expect to have
				 * a non-zero size value in the "pushtr".
				 */
				tupregs[ttop].dttk_size =
					dtrace_strlen(
						(char *)(uintptr_t)regs[rd],
						regs[r2]
						    ? regs[r2]
						    : dtrace_strsize_default
					) + 1;
			else
				tupregs[ttop].dttk_size = regs[r2];

			tupregs[ttop++].dttk_value = regs[rd];
			break;

		case DIF_OP_PUSHTV:
			if (ttop == DIF_DTR_NREGS) {
				*flags |= CPU_DTRACE_TUPOFLOW;
				break;
			}

			tupregs[ttop].dttk_value = regs[rd];
			tupregs[ttop++].dttk_size = 0;
			break;

		case DIF_OP_POPTS:
			if (ttop != 0)
				ttop--;
			break;

		case DIF_OP_FLUSHTS:
			ttop = 0;
			break;

		case DIF_OP_LDGAA:
		case DIF_OP_LDTAA: {
dtrace_dynvar_t *dvar;
dtrace_key_t *key = tupregs;
uint_t nkeys = ttop;

id = DIF_INSTR_VAR(instr);
ASSERT(id >= DIF_VAR_OTHER_UBASE);
id -= DIF_VAR_OTHER_UBASE;

key[nkeys].dttk_value = (uint64_t)id;
key[nkeys++].dttk_size = 0;

if (DIF_INSTR_OP(instr) == DIF_OP_LDTAA) {
DTRACE_TLS_THRKEY(key[nkeys].dttk_value);
key[nkeys++].dttk_size = 0;
v = &vstate->dtvs_tlocals[id];
} else
v = &vstate->dtvs_globals[id]->dtsv_var;

dvar = dtrace_dynvar(dstate, nkeys, key,
v->dtdv_type.dtdt_size > sizeof (uint64_t) ?
v->dtdv_type.dtdt_size : sizeof (uint64_t),
DTRACE_DYNVAR_NOALLOC, mstate, vstate);

if (dvar == NULL) {
regs[rd] = 0;
break;
}

if (v->dtdv_type.dtdt_flags & DIF_TF_BYREF)
regs[rd] = (uint64_t)(uintptr_t)dvar->dtdv_data;
else
regs[rd] = *((uint64_t *)dvar->dtdv_data);

break;
}

		case DIF_OP_STGAA:
		case DIF_OP_STTAA: {
dtrace_dynvar_t *dvar;
dtrace_key_t *key = tupregs;
uint_t nkeys = ttop;

id = DIF_INSTR_VAR(instr);
ASSERT(id >= DIF_VAR_OTHER_UBASE);
id -= DIF_VAR_OTHER_UBASE;

key[nkeys].dttk_value = (uint64_t)id;
key[nkeys++].dttk_size = 0;

if (DIF_INSTR_OP(instr) == DIF_OP_STTAA) {
DTRACE_TLS_THRKEY(key[nkeys].dttk_value);
key[nkeys++].dttk_size = 0;
v = &vstate->dtvs_tlocals[id];
} else {
v = &vstate->dtvs_globals[id]->dtsv_var;
}

dvar = dtrace_dynvar(dstate, nkeys, key,
v->dtdv_type.dtdt_size > sizeof (uint64_t) ?
v->dtdv_type.dtdt_size : sizeof (uint64_t),
regs[rd] ? DTRACE_DYNVAR_ALLOC :
DTRACE_DYNVAR_DEALLOC, mstate, vstate);

if (dvar == NULL)
break;

if (v->dtdv_type.dtdt_flags & DIF_TF_BYREF) {
if (!dtrace_vcanload(
(void *)(uintptr_t)regs[rd], &v->dtdv_type,
mstate, vstate))
break;

dtrace_vcopy((void *)(uintptr_t)regs[rd],
dvar->dtdv_data, &v->dtdv_type);
} else {
*((uint64_t *)dvar->dtdv_data) = regs[rd];
}

break;
}

		case DIF_OP_ALLOCS: {
uintptr_t ptr = P2ROUNDUP(mstate->dtms_scratch_ptr, 8);
size_t size = ptr - mstate->dtms_scratch_ptr + regs[r1];

/*
* Rounding up the user allocation size could have
* overflowed large, bogus allocations (like -1ULL) to
* 0.
*/
if (size < regs[r1] ||
!DTRACE_INSCRATCH(mstate, size)) {
DTRACE_CPUFLAG_SET(CPU_DTRACE_NOSCRATCH);
regs[rd] = NULL;
break;
}

dtrace_bzero((void *) mstate->dtms_scratch_ptr, size);
mstate->dtms_scratch_ptr += size;
regs[rd] = ptr;
break;
}

		case DIF_OP_COPYS:
if (!dtrace_canstore(regs[rd], regs[r2],
mstate, vstate)) {
*flags |= CPU_DTRACE_BADADDR;
*illval = regs[rd];
break;
}

if (!dtrace_canload(regs[r1], regs[r2], mstate, vstate))
break;

dtrace_bcopy((void *)(uintptr_t)regs[r1],
(void *)(uintptr_t)regs[rd], (size_t)regs[r2]);
break;

		case DIF_OP_STB:
if (!dtrace_canstore(regs[rd], 1, mstate, vstate)) {
*flags |= CPU_DTRACE_BADADDR;
*illval = regs[rd];
break;
}
*((uint8_t *)(uintptr_t)regs[rd]) = (uint8_t)regs[r1];
break;

		case DIF_OP_STH:
if (!dtrace_canstore(regs[rd], 2, mstate, vstate)) {
*flags |= CPU_DTRACE_BADADDR;
*illval = regs[rd];
break;
}
if (regs[rd] & 1) {
*flags |= CPU_DTRACE_BADALIGN;
*illval = regs[rd];
break;
}
*((uint16_t *)(uintptr_t)regs[rd]) = (uint16_t)regs[r1];
break;

		case DIF_OP_STW:
if (!dtrace_canstore(regs[rd], 4, mstate, vstate)) {
*flags |= CPU_DTRACE_BADADDR;
*illval = regs[rd];
break;
}
if (regs[rd] & 3) {
*flags |= CPU_DTRACE_BADALIGN;
*illval = regs[rd];
break;
}
*((uint32_t *)(uintptr_t)regs[rd]) = (uint32_t)regs[r1];
break;

		case DIF_OP_STX:
if (!dtrace_canstore(regs[rd], 8, mstate, vstate)) {
*flags |= CPU_DTRACE_BADADDR;
*illval = regs[rd];
break;
}
if (regs[rd] & 7) {
*flags |= CPU_DTRACE_BADALIGN;
*illval = regs[rd];
break;
}
*((uint64_t *)(uintptr_t)regs[rd]) = regs[r1];
break;
}
}

if (!(*flags & CPU_DTRACE_FAULT))
return (rval);

mstate->dtms_fltoffs = opc * sizeof (dif_instr_t);
mstate->dtms_present |= DTRACE_MSTATE_FLTOFFS;

return 0;
}
