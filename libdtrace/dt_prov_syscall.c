/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The syscall provider for DTrace.
 *
 * System call probes are exposed by the kernel as tracepoint events in the
 * "syscalls" group.  Entry probe names start with "sys_enter_" and exit probes
 * start with "sys_exit_".
 *
 * Mapping from event name to DTrace probe name:
 *
 *	syscalls:sys_enter_<name>		syscall:vmlinux:<name>:entry
 *	syscalls:sys_exit_<name>		syscall:vmlinux:<name>:return
 */
#include <assert.h>
#include <ctype.h>
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

static const char		prvname[] = "syscall";
static const char		modname[] = "vmlinux";

#define SYSCALLSFS		EVENTSFS "syscalls/"

/*
 * We need to skip over an extra field: __syscall_nr.
 */
#define SKIP_EXTRA_FIELDS	1

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

struct syscall_data {
	dt_pt_regs	*regs;
	long		syscall_nr;
	long		arg[6];
};

#define SCD_ARG(n)	offsetof(struct syscall_data, arg[n])

#define PROBE_LIST	TRACEFS "available_events"

#define PROV_PREFIX	"syscalls:"
#define ENTRY_PREFIX	"sys_enter_"
#define EXIT_PREFIX	"sys_exit_"

/* Scan the PROBE_LIST file and add probes for any syscalls events. */
static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_t	*prv;
	FILE		*f;
	char		buf[256];
	int		n = 0;

	prv = dt_provider_create(dtp, prvname, &dt_syscall, &pattr);
	if (prv == NULL)
		return 0;

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return 0;

	while (fgets(buf, sizeof(buf), f)) {
		char	*p;

		/* Here buf is "group:event".  */
		p = strchr(buf, '\n');
		if (p)
			*p = '\0';
		else {
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

		/* We need "group:" to match "syscalls:". */
		p = buf;
		if (memcmp(p, PROV_PREFIX, sizeof(PROV_PREFIX) - 1) != 0)
			continue;

		p += sizeof(PROV_PREFIX) - 1;

		/*
		 * Now p will be just "event", and we are only interested in
		 * events that match "sys_enter_*" or "sys_exit_*".
		 */
		if (!memcmp(p, ENTRY_PREFIX, sizeof(ENTRY_PREFIX) - 1)) {
			p += sizeof(ENTRY_PREFIX) - 1;
			if (tp_probe_insert(dtp, prv, prvname, modname, p,
					    "entry"))
				n++;
		} else if (!memcmp(p, EXIT_PREFIX, sizeof(EXIT_PREFIX) - 1)) {
			p += sizeof(EXIT_PREFIX) - 1;
			if (tp_probe_insert(dtp, prv, prvname, modname, p,
					    "return"))
				n++;
		}
	}

	fclose(f);

	return n;
}

/*
 * Generate a BPF trampoline for a syscall probe.
 *
 * The trampoline function is called when a syscall probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_syscall(struct syscall_data *scd)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements the compiled D clause.  It returns 0 to the caller.
 */
static void trampoline(dt_pcb_t *pcb)
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
	 *	memset(&dctx->mst->regs, 0, sizeof(dt_pt_regs);
	 *				// stdw [%7 + DMST_REGS + 0], 0
	 *				// stdw [%7 + DMST_REGS + 8], 0
	 *				//     (...)
	 */
	for (i = 0; i < sizeof(dt_pt_regs); i += 8) {
		instr = BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_REGS + i, 0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}
#endif

	/*
	 *	for (i = 0; i < argc; i++)
	 *		dctx->mst->argv[i] =
	 *			((struct syscall_data *)dctx->ctx)->arg[i];
	 *				// lddw %r0, [%r8 + SCD_ARG(i)]
	 *				// stdw [%r7 + DMST_ARG(i)], %r0
	 */
	for (i = 0; i < pcb->pcb_probe->argc; i++) {
		instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, SCD_ARG(i));
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(i), BPF_REG_0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	/*
	 *	for i = argc; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++)
	 *		dctx->mst->argv[i] = 0;
	 *				// stdw [%r7 + DMST_ARG(i)], 0
	 */
	for ( ; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++) {
		instr = BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(i), 0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	dt_cg_tramp_epilogue(pcb, lbl_exit);
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *argcp, dt_argdesc_t **argvp)
{
	FILE		*f;
	char		fn[256];
	int		rc;
	tp_probe_t	*datap = prp->prv_data;

	/* if we have an event ID, no need to retrieve it again */
	if (datap->event_id != -1)
		return -1;

	/*
	 * We know that the probe name is either "entry" or "return", so we can
	 * just check the first character.
	 */
	strcpy(fn, SYSCALLSFS);
	if (prp->desc->prb[0] == 'e')
		strcat(fn, "sys_enter_");
	else
		strcat(fn, "sys_exit_");
	strcat(fn, prp->desc->fun);
	strcat(fn, "/format");

	f = fopen(fn, "r");
	if (!f)
		return -ENOENT;

	rc = tp_event_info(dtp, f, SKIP_EXTRA_FIELDS, datap, argcp, argvp);
	fclose(f);

	return rc;
}

dt_provimpl_t	dt_syscall = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_TRACEPOINT,
	.populate	= &populate,
	.trampoline	= &trampoline,
	.attach		= &tp_attach,
	.probe_info	= &probe_info,
	.probe_destroy	= &tp_probe_destroy,
	.probe_fini	= &tp_probe_fini,
};
