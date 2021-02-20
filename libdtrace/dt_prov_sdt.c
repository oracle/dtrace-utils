/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The Statically Defined Tracepoint (SDT) provider for DTrace.
 *
 * SDT probes are exposed by the kernel as tracepoint events.  They are listed
 * in the TRACEFS/available_events file.
 *
 * Mapping from event name to DTrace probe name:
 *
 *	<group>:<name>				sdt:<group>::<name>
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/bpf.h>
#include <linux/perf_event.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <bpf_asm.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_bpf.h"
#include "dt_provider.h"
#include "dt_probe.h"
#include "dt_pt_regs.h"

static const char		prvname[] = "sdt";
static const char		modname[] = "vmlinux";

#define PROBE_LIST		TRACEFS "available_events"

#define KPROBES			"kprobes"
#define SYSCALLS		"syscalls"
#define UPROBES			"uprobes"

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

/*
 * The PROBE_LIST file lists all tracepoints in a <group>:<name> format.
 * We need to ignore these groups:
 *   - GROUP_FMT (created by DTrace processes)
 *   - kprobes and uprobes
 *   - syscalls (handled by a different provider)
 */
static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_t	*prv;
	FILE		*f;
	char		buf[256];
	char		*p;
	int		n = 0;

	prv = dt_provider_create(dtp, prvname, &dt_sdt, &pattr);
	if (prv == NULL)
		return 0;

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return 0;

	while (fgets(buf, sizeof(buf), f)) {
		int	dummy;
		char	str[sizeof(buf)];

		p = strchr(buf, '\n');
		if (p)
			*p = '\0';

		p = strchr(buf, ':');
		if (p != NULL) {
			size_t	len;

			*p++ = '\0';
			len = strlen(buf);

			if (sscanf(buf, GROUP_FMT, &dummy, str) == 2)
				continue;
			else if (len == strlen(KPROBES) &&
				 strcmp(buf, KPROBES) == 0)
				continue;
			else if (len == strlen(SYSCALLS) &&
				 strcmp(buf, SYSCALLS) == 0)
				continue;
			else if (len == strlen(UPROBES) &&
				 strcmp(buf, UPROBES) == 0)
				continue;

			if (dt_tp_probe_insert(dtp, prv, prvname, buf, "", p))
				n++;
		} else {
			if (dt_tp_probe_insert(dtp, prv, prvname, modname, "",
					    buf))
				n++;
		}
	}

	fclose(f);

	return n;
}

/*
 * Generate a BPF trampoline for a SDT probe.
 *
 * The trampoline function is called when a SDT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_sdt(struct syscall_data *scd)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements the compiled D clause.  It returns the value that it gets
 * back from that function.
 *
 * FIXME: Currently, access to arguments of the tracepoint is not supported.
 */
static void trampoline(dt_pcb_t *pcb)
{
	int		i;
	dt_irlist_t	*dlp = &pcb->pcb_ir;

	dt_cg_tramp_prologue(pcb);

	/*
	 * We cannot assume anything about the state of any registers so set up
	 * the ones we need:
	 *				//     (%r7 = dctx->mst)
	 *				// lddw %r7, [%fp + DCTX_FP(DCTX_MST)]
	 *				//     (%r8 = dctx->ctx)
	 *				// lddw %r8, [%fp + DCTX_FP(DCTX_CTX)]
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_7, BPF_REG_FP, DCTX_FP(DCTX_MST)));
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_8, BPF_REG_FP, DCTX_FP(DCTX_CTX)));

#if 0
	/*
	 *	memset(&dctx->mst->regs, 0, sizeof(dt_pt_regs);
	 *				// stdw [%7 + DMST_REGS + 0], 0
	 *				// stdw [%7 + DMST_REGS + 8], 0
	 *				//     (...)
	 */
	for (i = 0; i < sizeof(dt_pt_regs); i += 8) {
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_REGS + i, 0));
	}
#endif

	/*
	 *	for (i = 0; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++)
	 *		dctx->mst->argv[i] = 0
	 *				// stdw [%r7 + DMST_ARG(i)], 0
	 */
	for (i = 0; i < pcb->pcb_pinfo.dtp_argc; i++)
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(i), 0));

	dt_cg_tramp_epilogue(pcb);
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *argcp, dt_argdesc_t **argvp)
{
	FILE		*f;
	char		fn[256];
	int		rc;
	tp_probe_t	*tpp = prp->prv_data;

	/*
	 * If the tracepoint has already been created and we have its info,
	 * there is no need to retrive the info again.
	 */
	if (dt_tp_is_created(tpp))
		return -1;

	strcpy(fn, EVENTSFS);
	strcat(fn, prp->desc->mod);
	strcat(fn, "/");
	strcat(fn, prp->desc->prb);
	strcat(fn, "/format");

	f = fopen(fn, "r");
	if (!f)
		return -ENOENT;

	rc = dt_tp_event_info(dtp, f, 0, tpp, argcp, argvp);
	fclose(f);

	return rc;
}

dt_provimpl_t	dt_sdt = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_TRACEPOINT,
	.populate	= &populate,
	.trampoline	= &trampoline,
	.attach		= &dt_tp_probe_attach,
	.probe_info	= &probe_info,
	.detach		= &dt_tp_probe_detach,
	.probe_destroy	= &dt_tp_probe_destroy,
};
