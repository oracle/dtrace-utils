/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The Function Boundary Tracing (FBT) provider for DTrace.
 *
 * FBT probes are exposed by the kernel as kprobes.  They are listed in the
 * TRACEFS/available_filter_functions file.  Some kprobes are associated with
 * a specific kernel module, while most are in the core kernel.
 *
 * Mapping from event name to DTrace probe name:
 *
 *	<name>					fbt:vmlinux:<name>:entry
 *						fbt:vmlinux:<name>:return
 *   or
 *	<name> [<modname>]			fbt:<modname>:<name>:entry
 *						fbt:<modname>:<name>:return
 */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/bpf.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <bpf_asm.h>

#include "dt_impl.h"
#include "dt_bpf_builtins.h"
#include "dt_provider.h"
#include "dt_probe.h"
#include "dt_pt_regs.h"

static const char		prvname[] = "fbt";
static const char		modname[] = "vmlinux";

#define KPROBE_EVENTS		TRACEFS "kprobe_events"
#define PROBE_LIST		TRACEFS "available_filter_functions"

#define FBT_GROUP_FMT		GROUP_FMT "_%s"
#define FBT_GROUP_DATA		GROUP_DATA, prp->desc->prb

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

/*
 * Scan the PROBE_LIST file and add entry and return probes for every function
 * that is listed.
 */
static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_t		*prv;
	FILE			*f;
	char			buf[256];
	char			*p;
	const char		*mod = modname;
	int			n = 0;
	dtrace_syminfo_t	sip;
	dtrace_probedesc_t	pd;

	prv = dt_provider_create(dtp, prvname, &dt_fbt, &pattr);
	if (prv == NULL)
		return 0;

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return 0;

	while (fgets(buf, sizeof(buf), f)) {
		/*
		 * Here buf is either "funcname\n" or "funcname [modname]\n".
		 */
		p = strchr(buf, '\n');
		if (p) {
			*p = '\0';
			if (p > buf && *(--p) == ']')
				*p = '\0';
		} else {
			/*
			 * If we didn't see a newline, the line was too long.
			 * Report it, and skip until the end of the line.
			 */
			fprintf(stderr, "%s: Line too long: %s\n",
				PROBE_LIST, buf);

			do
				fgets(buf, sizeof(buf), f);
			while (strchr(buf, '\n') == NULL);
			continue;
		}

		/*
		 * Now buf is either "funcname" or "funcname [modname".  If
		 * there is no module name provided, we will use the default.
		 */
		p = strchr(buf, ' ');
		if (p) {
			*p++ = '\0';
			if (*p == '[')
				p++;
		}

		/*
		 * If we did not see a module name, perform a symbol lookup to
		 * try to determine the module name.
		 */
		if (!p) {
			if (dtrace_lookup_by_name(dtp, DTRACE_OBJ_KMODS, buf,
						  NULL, &sip) == 0)
				mod = sip.object;
		} else
			mod = p;

		/*
		 * Due to the lack of module names in
		 * TRACEFS/available_filter_functions, there are some duplicate
		 * function names.  We need to make sure that we do not create
		 * duplicate probes for these.
		 */
		pd.id = DTRACE_IDNONE;
		pd.prv = prvname;
		pd.mod = mod;
		pd.fun = buf;
		pd.prb = "entry";
		if (dt_probe_lookup(dtp, &pd) != NULL)
			continue;

		if (dt_probe_insert(dtp, prv, prvname, mod, buf, "entry"))
			n++;
		if (dt_probe_insert(dtp, prv, prvname, mod, buf, "return"))
			n++;
	}

	fclose(f);

	return n;
}

/*
 * Generate a BPF trampoline for a FBT probe.
 *
 * The trampoline function is called when a FBT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_fbt(dt_pt_regs *regs)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements the compiled D clause.  It returns 0 to the caller.
 */
static void trampoline(dt_pcb_t *pcb, const dt_ident_t *prog)
{
	int		i;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	struct bpf_insn	instr;
	uint_t		lbl_exit = dt_irlist_label(dlp);

	dt_cg_tramp_prologue(pcb, lbl_exit);

	/*
	 * We cannot assume anything about the state of any registers so set up
	 * the ones we need:
	 *				//     (%r7 = dctx->mst)
	 *				// lddw %r7, [%fp + DCTX_FP(DCTX_MST)]
	 *				//     (%r8 = dctx->ctx)
	 *				// lddw %r8, [%fp + DCTX_FP(DCTX_CTX)]
	 */
	instr = BPF_LOAD(BPF_DW, BPF_REG_7, BPF_REG_FP, DCTX_FP(DCTX_MST));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_8, BPF_REG_FP, DCTX_FP(DCTX_CTX));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

#if 0
	/*
	 *	dctx->mst->regs = *(dt_pt_regs *)dctx->ctx;
	 */
	for (i = 0; i < sizeof(dt_pt_regs); i += 8) {
		instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, i);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_REGS + i, BPF_REG_0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}
#endif

	/*
	 *	dctx->mst->argv[0] = PT_REGS_PARAM1((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG0]
	 *				// stdw [%r7 + DMST_ARG(0)0], %r0
	 *	dctx->mst->argv[1] = PT_REGS_PARAM2((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG1]
	 *				// stdw [%r7 + DMST_ARG(1)8], %r0
	 *	dctx->mst->argv[2] = PT_REGS_PARAM3((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG2]
	 *				// stdw [%r7 + DMST_ARG(2)8], %r0
	 *	dctx->mst->argv[3] = PT_REGS_PARAM4((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG3]
	 *				// stdw [%r7 + DMST_ARG(3)8], %r0
	 *	dctx->mst->argv[4] = PT_REGS_PARAM5((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG4]
	 *				// stdw [%r7 + DMST_ARG(4)], %r0
	 *	dctx->mst->argv[5] = PT_REGS_PARAM6((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG5]
	 *				// stdw [%r7 + DMST_ARG(5)], %r0
	 */
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG2);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(2), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG3);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(3), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG4);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(4), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG5);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(5), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *     (we clear dctx->mst->argv[6] and on)
	 */
	for (i = 6; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++) {
		instr = BPF_STORE_IMM(BPF_DW, BPF_REG_7, DCTX_FP(DMST_ARG(i)),
				      0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	dt_cg_tramp_epilogue(pcb, prog, lbl_exit);
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *idp, int *argcp, dt_argdesc_t **argvp)
{
	int	fd;
	char	*fn;
	size_t	len;
	FILE	*f;
	int	rc = -1;

	*idp = -1;
	*argcp = 0;			/* no arguments by default */
	*argvp = NULL;

	/*
	 * Register the kprobe with the tracing subsystem.  This will
	 * create a tracepoint event.
	 */
	fd = open(KPROBE_EVENTS, O_WRONLY | O_APPEND);
	if (fd == -1)
		return -1;

	rc = dprintf(fd, "%c:" FBT_GROUP_FMT "/%s %s\n",
		     prp->desc->prb[0] == 'e' ? 'p' : 'r', FBT_GROUP_DATA,
		     prp->desc->fun, prp->desc->fun);
	close(fd);
	if (rc == -1)
		return -1;

	len = snprintf(NULL, 0, "%s" FBT_GROUP_FMT "/%s/format", EVENTSFS,
		       FBT_GROUP_DATA, prp->desc->fun) + 1;
	fn = dt_alloc(dtp, len);
	if (fn == NULL)
		goto out;

	snprintf(fn, len, "%s" FBT_GROUP_FMT "/%s/format", EVENTSFS,
		 FBT_GROUP_DATA, prp->desc->fun);
	f = fopen(fn, "r");
	if (f == NULL)
		goto out;

	rc = tp_event_info(dtp, f, 0, idp, NULL, NULL);
	fclose(f);

out:
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

	fd = open(KPROBE_EVENTS, O_WRONLY | O_APPEND);
	if (fd == -1)
		return -1;

	dprintf(fd, "-:" FBT_GROUP_FMT "/%s\n", FBT_GROUP_DATA,
		prp->desc->fun);
	close(fd);

	return 0;
}

dt_provimpl_t	dt_fbt = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_KPROBE,
	.populate	= &populate,
	.trampoline	= &trampoline,
	.probe_info	= &probe_info,
	.probe_fini	= &probe_fini,
};
