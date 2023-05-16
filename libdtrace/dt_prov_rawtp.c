/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The Raw Tracepoint provider for DTrace.
 *
 * Raw tracepoints are exposed by the kernel tracing system to allow access to
 * untranslated arguments to their associated tracepoint events.  Each
 * tracepoint event listed in the TRACEFS/available_events file can be traced
 * as a raw tracepoint using the BPF program type BPF_PROG_TYPE_RAW_TRACEPOINT.
 *
 * Mapping from event name to DTrace probe name:
 *
 *	<group>:<name>				rawtp:<group>::<name>
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
#include "dt_bpf.h"
#include "dt_cg.h"
#include "dt_bpf.h"
#include "dt_provider_tp.h"
#include "dt_probe.h"
#include "dt_pt_regs.h"

static const char		prvname[] = "rawtp";
static const char		modname[] = "vmlinux";

#define PROBE_LIST		TRACEFS "available_events"

#define KPROBES			"kprobes"
#define SYSCALLS		"syscalls"
#define UPROBES			"uprobes"
#define PID			"dt_pid"

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
 *   - pid and usdt probes (ditto)
 */
static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_t	*prv;
	FILE		*f;
	char		*buf = NULL;
	char		*p;
	size_t		n;

	prv = dt_provider_create(dtp, prvname, &dt_rawtp, &pattr, NULL);
	if (prv == NULL)
		return 0;

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return 0;

	while (getline(&buf, &n, f) >= 0) {
		p = strchr(buf, '\n');
		if (p)
			*p = '\0';

		p = strchr(buf, ':');
		if (p != NULL) {
			int	dummy;
			char	*str;

			*p++ = '\0';

			if (sscanf(buf, GROUP_SFMT, &dummy, &str) == 2) {
				free(str);
				continue;
			}
			else if (strcmp(buf, KPROBES) == 0)
				continue;
			else if (strcmp(buf, SYSCALLS) == 0)
				continue;
			else if (strcmp(buf, UPROBES) == 0)
				continue;
			else if (strcmp(buf, PID) == 0)
				continue;

			if (dt_tp_probe_insert(dtp, prv, prvname, buf, "", p))
				n++;
		} else {
			if (dt_tp_probe_insert(dtp, prv, prvname, modname, "",
					    buf))
				n++;
		}
	}

	free(buf);
	fclose(f);

	return n;
}

/*
 * Generate a BPF trampoline for a SDT probe.
 *
 * The trampoline function is called when a SDT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_rawtp(void *data)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements the compiled D clause.  It returns the value that it gets
 * back from that function.
 */
static int trampoline(dt_pcb_t *pcb, uint_t exitlbl)
{
	int		i;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_probe_t	*prp = pcb->pcb_probe;

	dt_cg_tramp_prologue(pcb);

	/*
	 * After the dt_cg_tramp_prologue() call, we have:
	 *				//     (%r7 = dctx->mst)
	 *				//     (%r8 = dctx->ctx)
	 */

	dt_cg_tramp_clear_regs(pcb);

	for (i = 0; i < prp->argc; i++) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, i * sizeof(uint64_t)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(i), BPF_REG_0));
	}

	dt_cg_tramp_epilogue(pcb);

	return 0;
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *argcp, dt_argdesc_t **argvp)
{
	int		argc, i;
	dt_argdesc_t	*argv = NULL;

	/*
	 * This is an unfortunate necessity.  The BPF verifier will not allow
	 * us to access more argument values than are passed to the raw
	 * tracepoint but the number of argument values for any given raw
	 * tracepoint is not made available to userspace.  So we use a trial
	 * and error loop to see what the BPF verifier accepts.
	 */
	for (argc = ARRAY_SIZE(((dt_mstate_t *)0)->argv); argc > 0; argc--) {
		int		bpf_fd, rtp_fd;
		struct bpf_insn	prog[2];
		dtrace_difo_t	dif;

		prog[0] = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, (argc - 1) * sizeof(uint64_t));
		prog[1] = BPF_RETURN();
		dif.dtdo_buf = prog;
		dif.dtdo_len = 2;

		bpf_fd = dt_bpf_prog_load(dt_rawtp.prog_type, &dif, 0, NULL, 0);
		if (bpf_fd == -1)
			continue;
		rtp_fd = dt_bpf_raw_tracepoint_open(prp->desc->prb, bpf_fd);
		close(bpf_fd);
		if (rtp_fd == -1)
			continue;
		close(rtp_fd);
		break;
	}

	if (argc == 0)
		goto done;

	argv = dt_zalloc(dtp, argc * sizeof(dt_argdesc_t));

	for (i = 0; i < argc; i++) {
		argv[i].mapping = i;
		argv[i].native = "uint64_t";
		argv[i].xlate = NULL;
	}

done:
        *argcp = argc;
        *argvp = argv;

        return 0;
}

dt_provimpl_t	dt_rawtp = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_RAW_TRACEPOINT,
	.populate	= &populate,
	.trampoline	= &trampoline,
	.attach		= &dt_tp_probe_attach_raw,
	.probe_info	= &probe_info,
	.detach		= &dt_tp_probe_detach,
	.probe_destroy	= &dt_tp_probe_destroy,
};
