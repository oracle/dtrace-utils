/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The core probe provider for DTrace for the BEGIN, END, and ERROR probes.
 */
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <bpf_asm.h>

#include "dt_provider.h"
#include "dt_probe.h"

static const char		prvname[] = "dtrace";
static const char		modname[] = "";
static const char		funname[] = "";

#define PROBE_FUNC_SUFFIX	"_probe"

#define UPROBE_EVENTS		TRACEFS "uprobe_events"

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
};

static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_t	*prv;
	int		n = 0;


	prv = dt_provider_create(dtp, prvname, &dt_dtrace, &pattr);
	if (prv == NULL)
		return 0;

	if (dt_probe_insert(dtp, prv, prvname, modname, funname, "BEGIN"))
		n++;
	if (dt_probe_insert(dtp, prv, prvname, modname, funname, "END"))
		n++;
	if (dt_probe_insert(dtp, prv, prvname, modname, funname, "ERROR"))
		n++;

	return n;
}

/*
 * Generate a BPF trampoline for a dtrace probe (BEGIN, END, or ERROR).
 *
 * The trampoline function is called when a dtrace probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_dtrace(dt_pt_regs *regs)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements tha compiled D clause.  It returns the value that it gets
 * back from that function.
 */
static void trampoline(dt_pcb_t *pcb)
{
	int		i;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	struct bpf_insn	instr;
	uint_t		lbl_exit = dt_irlist_label(dlp);
	dt_ident_t	*idp;

#define DCTX_FP(off)	(-(ushort_t)DCTX_SIZE + (ushort_t)(off))

	/*
	 * int dt_dtrace(dt_pt_regs *regs)
	 * {
	 *     dt_dctx_t	dctx;
	 *
	 *     memset(&dctx, 0, sizeof(dctx));
	 *
	 *     dctx.epid = EPID;
	 *     (we clear dctx.pad and dctx.fault because of the memset above)
	 */
	idp = dt_dlib_get_var(pcb->pcb_hdl, "EPID");
	assert(idp != NULL);
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_FP, DCTX_FP(DCTX_EPID), -1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dlp->dl_last->di_extern = idp;
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_FP, DCTX_FP(DCTX_PAD), 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE_IMM(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_FAULT), 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

#if 0
	/*
	 *     dctx.regs = *regs;
	 */
	for (i = 0; i < sizeof(dt_pt_regs); i += 8) {
		instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, i);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_REGS) + i,
				  BPF_REG_0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}
#endif

	/*
	 *     dctx.argv[0] = PT_REGS_PARAM1(regs);
	 *     dctx.argv[1] = PT_REGS_PARAM2(regs);
	 *     dctx.argv[2] = PT_REGS_PARAM3(regs);
	 *     dctx.argv[3] = PT_REGS_PARAM4(regs);
	 *     dctx.argv[4] = PT_REGS_PARAM5(regs);
	 *     dctx.argv[5] = PT_REGS_PARAM6(regs);
	 */
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(0)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(1)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG2);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(2)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG3);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(3)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG4);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(4)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG5);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(5)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *     (we clear dctx.argv[6] and on because of the memset above)
	 */
	for (i = 6; i < ARRAY_SIZE(((dt_dctx_t *)0)->argv); i++) {
		instr = BPF_STORE_IMM(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(i)),
				      0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	/*
	 * We know the BPF context (regs) is in %r1.  Since we will be passing
	 * the DTrace context (dctx) as 2nd argument to dt_predicate() (if
	 * there is a predicate) and dt_program, we need it in %r2.
	 */
	instr = BPF_MOV_REG(BPF_REG_2, BPF_REG_FP);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_FP(0));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *     rc = dt_program(regs, dctx);
	 */
	idp = dt_dlib_get_func(pcb->pcb_hdl, "dt_program");
	assert(idp != NULL);
	instr = BPF_CALL_FUNC(idp->di_id);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dlp->dl_last->di_extern = idp;

	/*
	 * exit:
	 *     return rc;
	 * }
	 */
	instr = BPF_RETURN();
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_exit, instr));
}

static char *uprobe_spec(dtrace_hdl_t *dtp, const char *prb)
{
	struct ps_prochandle	*P;
	int			perr = 0;
	char			*fun;
	GElf_Sym		sym;
	prsyminfo_t		si;
	char			*spec = NULL;

	fun = dt_alloc(dtp, strlen(prb) + strlen(PROBE_FUNC_SUFFIX) + 1);
	if (fun == NULL)
		return NULL;

	strcpy(fun, prb);
	strcat(fun, PROBE_FUNC_SUFFIX);

	/* grab our process */
	P = Pgrab(getpid(), 2, 0, NULL, &perr);
	if (P == NULL) {
		dt_free(dtp, fun);
		return NULL;
	}

	/* look up function, get the map, and record */
	if (Pxlookup_by_name(P, -1, PR_OBJ_EVERY, fun, &sym, &si) == 0) {
		const prmap_t	*mapp;
		size_t		len;

		mapp = Paddr_to_map(P, sym.st_value);
		if (mapp == NULL)
			goto out;

		if (mapp->pr_file->first_segment != mapp)
			mapp = mapp->pr_file->first_segment;

		len = snprintf(NULL, 0, "%s:0x%lx",
			       mapp->pr_file->prf_mapname,
			       sym.st_value - mapp->pr_vaddr) + 1;
		spec = dt_alloc(dtp, len);
		if (spec == NULL)
			goto out;

		snprintf(spec, len, "%s:0x%lx", mapp->pr_file->prf_mapname,
			 sym.st_value - mapp->pr_vaddr);
	}

out:
	dt_free(dtp, fun);
	Prelease(P, PS_RELEASE_NORMAL);
	Pfree(P);

	return spec;
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *idp, int *argcp, dt_argdesc_t **argvp)
{
	char	*spec;
	char	*fn = NULL;
	int	fd;
	FILE	*f;
	int	rc = -ENOENT;
	size_t	len;

	*idp = -1;
	*argcp = 0;			/* no arguments */
	*argvp = NULL;

	/* get a uprobe specification for this probe */
	spec = uprobe_spec(dtp, prp->desc->prb);
	if (spec == NULL)
		return -ENOENT;

	/* add a uprobe */
	fd = open(UPROBE_EVENTS, O_WRONLY | O_APPEND);
	if (fd == -1)
		goto out;

	rc = dprintf(fd, "p:" GROUP_FMT "/%s %s\n",
		     GROUP_DATA, prp->desc->prb, spec);
	close(fd);
	if (rc == -1)
		goto out;

	len = snprintf(NULL, 0, "%s" GROUP_FMT "/%s/format",
		       EVENTSFS, GROUP_DATA, prp->desc->prb) + 1;
	fn = dt_alloc(dtp, len);
	if (fn == NULL)
		goto out;

	snprintf(fn, len, "%s" GROUP_FMT "/%s/format",
		 EVENTSFS, GROUP_DATA, prp->desc->prb);
	f = fopen(fn, "r");
	if (f == NULL)
		goto out;

	rc = tp_event_info(dtp, f, 0, idp, NULL, NULL);
	fclose(f);

out:
	dt_free(dtp, spec);
	dt_free(dtp, fn);

	return rc;
}

/*
 * Try to clean up system resources that may have been allocated for this
 * probe.
 *
 * If there is an event FD, we close it.
 *
 * We also try to remove any uprobe that may have been created for the probe.
 * This is harmless for probes that didn't get created.  If the removal fails
 * for some reason we are out of luck - fortunately it is not harmful to the
 * system as a whole.
 */
static int probe_fini(dtrace_hdl_t *dtp, dt_probe_t *prp)
{
	int	fd;

	if (prp->event_fd != -1) {
		close(prp->event_fd);
		prp->event_fd = -1;
	}

	fd = open(UPROBE_EVENTS, O_WRONLY | O_APPEND);
	if (fd == -1)
		return -1;

	dprintf(fd, "-:" GROUP_FMT "/%s\n", GROUP_DATA, prp->desc->prb);
	close(fd);

	return 0;
}

dt_provimpl_t	dt_dtrace = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_KPROBE,
	.populate	= &populate,
	.trampoline	= &trampoline,
	.probe_info	= &probe_info,
	.probe_fini	= &probe_fini,
};
