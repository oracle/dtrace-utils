/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The Raw Function Boundary Tracing provider for DTrace.
 *
 * The kernel provides kprobes to trace specific symbols.  They are listed in
 * the TRACEFS/available_filter_functions file.  Kprobes may be associated with
 * a symbol in the core kernel or with a symbol in a specific kernel module.
 * Whereas the fbt provider supports tracing regular symbols only, the rawfbt
 * provider also provides access to synthetic symbols, i.e. symbols created by
 * compiler optimizations.
 *
 * Mapping from event name to DTrace probe name:
 *
 *	<name>					rawfbt:vmlinux:<name>:entry
 *						rawfbt:vmlinux:<name>:return
 *   or
 *	<name> [<modname>]			rawfbt:<modname>:<name>:entry
 *						rawfbt:<modname>:<name>:return
 */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/bpf.h>
#include <linux/btf.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <bpf_asm.h>

#include "dt_btf.h"
#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_module.h"
#include "dt_provider_tp.h"
#include "dt_probe.h"
#include "dt_pt_regs.h"

static const char		prvname[] = "rawfbt";
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
	char			*buf = NULL;
	size_t			len = 0;
	size_t			n = 0;
	dtrace_syminfo_t	sip;
	dtrace_probedesc_t	pd;

	prv = dt_provider_create(dtp, prvname, &dt_rawfbt, &pattr, NULL);
	if (prv == NULL)
		return -1;			/* errno already set */

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return 0;

	while (getline(&buf, &len, f) >= 0) {
		char		*p;
		const char	*mod = modname;
		dt_probe_t	*prp;

		/*
		 * Here buf is either "funcname\n" or "funcname [modname]\n".
		 * The last line may not have a linefeed.
		 */
		p = strchr(buf, '\n');
		if (p) {
			*p = '\0';
			if (p > buf && *(--p) == ']')
				*p = '\0';
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

#define strstarts(var, x) (strncmp(var, x, strlen (x)) == 0)
		/* Weed out __ftrace_invalid_address___* entries. */
		if (strstarts(buf, "__ftrace_invalid_address__") ||
		    strstarts(buf, "__probestub_") ||
		    strstarts(buf, "__traceiter_"))
			continue;
#undef strstarts

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
		 * function names.  The kernel does not let us trace functions
		 * that have duplicates, so we need to remove the existing one.
		 */
		pd.id = DTRACE_IDNONE;
		pd.prv = prvname;
		pd.mod = mod;
		pd.fun = buf;
		pd.prb = "entry";
		prp = dt_probe_lookup(dtp, &pd);
		if (prp != NULL) {
			dt_probe_destroy(prp);
			continue;
		}

		if (dt_tp_probe_insert(dtp, prv, prvname, mod, buf, "entry"))
			n++;
		if (dt_tp_probe_insert(dtp, prv, prvname, mod, buf, "return"))
			n++;
	}

	free(buf);
	fclose(f);

	return n;
}

/*
 * Generate a BPF trampoline for a FBT probe.
 *
 * The trampoline function is called when a FBT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_rawfbt(dt_pt_regs *regs)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements the compiled D clause.  It returns 0 to the caller.
 */
static int trampoline(dt_pcb_t *pcb, uint_t exitlbl)
{
	dt_cg_tramp_prologue(pcb);

	/*
	 * After the dt_cg_tramp_prologue() call, we have:
	 *				//     (%r7 = dctx->mst)
	 *				//     (%r8 = dctx->ctx)
	 */
	dt_cg_tramp_copy_regs(pcb);
	if (strcmp(pcb->pcb_probe->desc->prb, "return") == 0) {
		dt_irlist_t	*dlp = &pcb->pcb_ir;

		dt_cg_tramp_copy_rval_from_regs(pcb);

		/*
		 * fbt:::return arg0 should be the function offset for
		 * return instruction.  Since we use kretprobes, however,
		 * which do not fire until the function has returned to
		 * its caller, information about the returning instruction
		 * in the callee has been lost.
		 *
		 * Set arg0=-1 to indicate that we do not know the value.
		 */
		dt_cg_xsetx(dlp, NULL, DT_LBL_NONE, BPF_REG_0, -1);
		emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
	} else
		dt_cg_tramp_copy_args_from_regs(pcb, 1);
	dt_cg_tramp_epilogue(pcb);

	return 0;
}

static int attach(dtrace_hdl_t *dtp, const dt_probe_t *prp, int bpf_fd)
{
	char	*prb = NULL;

	if (!dt_tp_probe_has_info(prp)) {
		char	*fn, *p;
		FILE	*f;
		int	fd, rc = -1;

		/*
		 * The tracepoint event we will be creating needs to have a
		 * valid name.  We use a copy of the probe name, with . -> _
		 * conversion.
		 */
		prb = strdup(prp->desc->fun);
		for (p = prb; *p; p++) {
			if (*p == '.')
				*p = '_';
		}

		/*
		 * Register the kprobe with the tracing subsystem.  This will
		 * create a tracepoint event.
		 */
		fd = open(KPROBE_EVENTS, O_WRONLY | O_APPEND);
		if (fd == -1)
			goto fail;

		rc = dprintf(fd, "%c:" FBT_GROUP_FMT "/%s %s\n",
			     prp->desc->prb[0] == 'e' ? 'p' : 'r',
			     FBT_GROUP_DATA, prb, prp->desc->fun);
		close(fd);
		if (rc == -1)
			goto fail;

		/* create format file name */
		if (asprintf(&fn, "%s" FBT_GROUP_FMT "/%s/format", EVENTSFS,
			     FBT_GROUP_DATA, prb) == -1)
			goto fail;

		/* open format file */
		f = fopen(fn, "r");
		free(fn);
		if (f == NULL)
			goto fail;

		/* read event id from format file */
		rc = dt_tp_probe_info(dtp, f, 0, prp, NULL, NULL);
		fclose(f);

		if (rc < 0)
			goto fail;

		free(prb);
	}

	/* attach BPF program to the probe */
	return dt_tp_probe_attach(dtp, prp, bpf_fd);

fail:
	free(prb);
	return -ENOENT;
}

/*
 * Try to clean up system resources that may have been allocated for this
 * probe.
 *
 * If there is an event FD, we close it.
 *
 * We also try to remove any kprobe that may have been created for the probe.
 * This is harmless for probes that didn't get created.  If the removal fails
 * for some reason we are out of luck - fortunately it is not harmful to the
 * system as a whole.
 */
static void detach(dtrace_hdl_t *dtp, const dt_probe_t *prp)
{
	int	fd;
	char	*prb, *p;

	if (!dt_tp_probe_has_info(prp))
		return;

	dt_tp_probe_detach(dtp, prp);

	fd = open(KPROBE_EVENTS, O_WRONLY | O_APPEND);
	if (fd == -1)
		return;

	/* The tracepoint event is the probe nam, with . -> _ conversion. */
	prb = strdup(prp->desc->fun);
	for (p = prb; *p; p++) {
		if (*p == '.')
			*p = '_';
	}

	dprintf(fd, "-:" FBT_GROUP_FMT "/%s\n", FBT_GROUP_DATA, prb);
	free(prb);
	close(fd);
}

dt_provimpl_t	dt_rawfbt = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_KPROBE,
	.populate	= &populate,
	.load_prog	= &dt_bpf_prog_load,
	.trampoline	= &trampoline,
	.attach		= &attach,
	.detach		= &detach,
	.probe_destroy	= &dt_tp_probe_destroy,
};
