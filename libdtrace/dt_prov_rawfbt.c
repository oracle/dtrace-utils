/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, 2025, Oracle and/or its affiliates. All rights reserved.
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

#define KPROBE_EVENTS		TRACEFS "kprobe_events"

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
 * Create the rawfbt provider.
 */
static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_t		*prv;

	prv = dt_provider_create(dtp, prvname, &dt_rawfbt, &pattr, NULL);
	if (prv == NULL)
		return -1;			/* errno already set */

	return 0;
}

/* Create a probe (if it does not exist yet). */
static int provide_probe(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp)
{
	dt_provider_t	*prv = dt_provider_lookup(dtp, pdp->prv);

	if (prv == NULL)
		return 0;
	if (dt_probe_lookup(dtp, pdp) != NULL)
		return 0;
	if (dt_tp_probe_insert(dtp, prv, pdp->prv, pdp->mod, pdp->fun, pdp->prb))
		return 1;

	return 0;
}

/*
 * Try to provide probes for the given probe description.  The caller ensures
 * that the provider name in probe desxcription (if any) is a match for this
 * provider.  When this is called, we already know that this provider matches
 * the provider component of the probe specification.
 */
#define FBT_ENTRY	1
#define FBT_RETURN	2

static int provide(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp)
{
	int			n = 0;
	int			prb = 0;
	dt_module_t		*dmp = NULL;
	dt_symbol_t		*sym = NULL;
	dt_htab_next_t		*it = NULL;
	dtrace_probedesc_t	pd;

	dt_modsym_mark_traceable(dtp);

	/*
	 * Nothing to do if a probe name is specified and cannot match 'entry'
	 * or 'return'.
	 */
	if (dt_gmatch("entry", pdp->prb))
		prb |= FBT_ENTRY;
	if (dt_gmatch("return", pdp->prb))
		prb |= FBT_RETURN;
	if (prb == 0)
		return 0;

	/*
	 * If we have an explicit module name, check it.  If not found, we can
	 * ignore this request.
	 */
	if (pdp->mod[0] != '\0' && strchr(pdp->mod, '*') == NULL) {
		dmp = dt_module_lookup_by_name(dtp, pdp->mod);
		if (dmp == NULL)
			return 0;
	}

	/*
	 * If we have an explicit function name, we start with a basic symbol
	 * name lookup.
	 */
	if (pdp->fun[0] != '\0' && strchr(pdp->fun, '*') == NULL) {
		/* If we have a module, use it. */
		if (dmp != NULL) {
			sym = dt_module_symbol_by_name(dtp, dmp, pdp->fun);
			if (sym == NULL)
				return 0;
			if (!dt_symbol_traceable(sym))
				return 0;

			pd.id = DTRACE_IDNONE;
			pd.prv = pdp->prv;
			pd.mod = dmp->dm_name;
			pd.fun = pdp->fun;

			if (prb & FBT_ENTRY) {
				pd.prb = "entry";
				n += provide_probe(dtp, &pd);
			}
			if (prb & FBT_RETURN) {
				pd.prb = "return";
				n += provide_probe(dtp, &pd);
			}

			return n;
		}

		sym = dt_symbol_by_name(dtp, pdp->fun);
		while (sym != NULL) {
			const char	*mod = dt_symbol_module(sym)->dm_name;

			if (dt_symbol_traceable(sym) &&
			    dt_gmatch(mod, pdp->mod)) {
				pd.id = DTRACE_IDNONE;
				pd.prv = pdp->prv;
				pd.mod = mod;
				pd.fun = pdp->fun;

				if (prb & FBT_ENTRY) {
					pd.prb = "entry";
					n += provide_probe(dtp, &pd);
				}
				if (prb & FBT_RETURN) {
					pd.prb = "return";
					n += provide_probe(dtp, &pd);
				}

			}
			sym = dt_symbol_by_name_next(sym);
		}

		return n;
	}

	/*
	 * No explicit function name.  We need to go through all possible
	 * symbol names and see if they match.
	 */
	while ((sym = dt_htab_next(dtp->dt_kernsyms, &it)) != NULL) {
		dt_module_t	*smp;
		const char	*fun;

		/* Ensure the symbol can be traced. */
		if (!dt_symbol_traceable(sym))
			continue;

		/* Match the function name. */
		fun = dt_symbol_name(sym);
		if (!dt_gmatch(fun, pdp->fun))
			continue;

		/* Validate the module name. */
		smp = dt_symbol_module(sym);
		if (dmp) {
			if (smp != dmp)
				continue;
		} else if (!dt_gmatch(smp->dm_name, pdp->mod))
			continue;

		pd.id = DTRACE_IDNONE;
		pd.prv = pdp->prv;
		pd.mod = smp->dm_name;
		pd.fun = fun;

		if (prb & FBT_ENTRY) {
			pd.prb = "entry";
			n += provide_probe(dtp, &pd);
		}
		if (prb & FBT_RETURN) {
			pd.prb = "return";
			n += provide_probe(dtp, &pd);
		}
	}

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
	.provide	= &provide,
	.load_prog	= &dt_bpf_prog_load,
	.trampoline	= &trampoline,
	.attach		= &attach,
	.detach		= &detach,
	.probe_destroy	= &dt_tp_probe_destroy,
};
