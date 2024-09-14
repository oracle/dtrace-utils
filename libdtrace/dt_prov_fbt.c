/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates. All rights reserved.
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
	dt_provimpl_t		*impl;
	FILE			*f;
	char			*buf = NULL;
	char			*p;
	const char		*mod = modname;
	size_t			n;
	dtrace_syminfo_t	sip;
	dtrace_probedesc_t	pd;

	impl = BPF_HAS(dtp, BPF_FEAT_FENTRY) ? &dt_fbt_fprobe : &dt_fbt_kprobe;

	prv = dt_provider_create(dtp, prvname, impl, &pattr, NULL);
	if (prv == NULL)
		return -1;			/* errno already set */

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return 0;

	while (getline(&buf, &n, f) >= 0) {
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

		/* Weed out synthetic symbol names (that are invalid). */
		if (strchr(buf, '.') != NULL)
			continue;

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

		if (dt_tp_probe_insert(dtp, prv, prvname, mod, buf, "entry"))
			n++;
		if (dt_tp_probe_insert(dtp, prv, prvname, mod, buf, "return"))
			n++;
	}

	free(buf);
	fclose(f);

	return n;
}

/*******************************\
 * FPROBE-based implementation *
\*******************************/

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
static int fprobe_trampoline(dt_pcb_t *pcb, uint_t exitlbl)
{
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_probe_t	*prp = pcb->pcb_probe;

	dt_cg_tramp_prologue(pcb);

	if (strcmp(pcb->pcb_probe->desc->prb, "entry") == 0) {
		int	i;

		for (i = 0; i < prp->argc; i++) {
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, i * 8));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(i), BPF_REG_0));
		}
	} else {
		dt_module_t	*dmp;

		/*
		 * fbt:::return arg0 should be the function offset for the
		 * return instruction.  The fexit probe fires at a point where
		 * we can no longer determine this location.
		 *
		 * Set arg0 = -1 to indicate that we do not know the value.
		 */
		dt_cg_xsetx(dlp, NULL, DT_LBL_NONE, BPF_REG_0, -1);
		emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));

		/*
		 * Only try to retrieve a return value if we know we can.
		 *
		 * The return value is provided by the fexit probe as an
		 * argument slot past the last function argument.  We can get
		 * the number of function arguments using the BTF id that has
		 * been stored as the tracepoint event id.
		 */
		dmp = dt_module_lookup_by_name(dtp, prp->desc->mod);
		if (dmp && prp->argc == 2) {
			int32_t	btf_id = dt_tp_get_event_id(prp);
			int	i = dt_btf_func_argc(dtp, dmp->dm_btf, btf_id);

			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, i * 8));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0));
		}
	}

	dt_cg_tramp_epilogue(pcb);

	return 0;
}

static int fprobe_probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
			     int *argcp, dt_argdesc_t **argvp)
{
	const dtrace_probedesc_t	*desc = prp->desc;
	dt_module_t			*dmp;
	int32_t				btf_id;
	int				i, argc = 0;
	dt_argdesc_t			*argv = NULL;

	dmp = dt_module_lookup_by_name(dtp, desc->mod);
	if (dmp == NULL)
		goto done;

	btf_id = dt_btf_lookup_name_kind(dtp, dmp->dm_btf, desc->fun,
					 BTF_KIND_FUNC);
	if (btf_id <= 0)
		goto done;

	dt_tp_set_event_id(prp, btf_id);

	if (strcmp(desc->prb, "return") == 0) {
		/* Void function return probes only provide 1 argument. */
		argc = dt_btf_func_is_void(dtp, dmp->dm_btf, btf_id) ? 1 : 2;

		argv = dt_calloc(dtp, argc, sizeof(dt_argdesc_t));
		if (argv == NULL)
			return dt_set_errno(dtp, EDT_NOMEM);

		/* The return offset is in args[0] (always -1). */
		argv[0].mapping = 0;
		argv[0].native = strdup("uint64_t");
		argv[0].xlate = NULL;

		if (argc == 2) {
			/* The return type is a generic uint64_t. */
			argv[1].mapping = 1;
			argv[1].native = strdup("uint64_t");
			argv[1].xlate = NULL;
		}

		goto done;
	}

	argc = dt_btf_func_argc(dtp, dmp->dm_btf, btf_id);
	if (argc == 0)
		goto done;

	argv = dt_calloc(dtp, argc, sizeof(dt_argdesc_t));
	if (argv == NULL)
		return dt_set_errno(dtp, EDT_NOMEM);

	/* In the absence of argument type info, use uint64_t for all. */
	for (i = 0; i < argc; i++) {
		argv[i].mapping = i;
		argv[i].native = strdup("uint64_t");
		argv[i].xlate = NULL;
	}

done:
	*argcp = argc;
	*argvp = argv;

	return 0;
}

static int fprobe_prog_load(dtrace_hdl_t *dtp, const dt_probe_t *prp,
			    const dtrace_difo_t *dp, uint32_t lvl, char *buf,
			    size_t sz)
{
	int				atype, fd, rc;
	const dtrace_probedesc_t	*desc = prp->desc;
	dt_module_t			*dmp;

	atype = strcmp(desc->prb, "entry") == 0 ? BPF_TRACE_FENTRY
						: BPF_TRACE_FEXIT;

	dmp = dt_module_lookup_by_name(dtp, desc->mod);
	if (dmp == NULL)
		return dt_set_errno(dtp, EDT_NOMOD);

	fd = dt_btf_module_fd(dmp);
	if (fd < 0)
		return fd;

	rc = dt_bpf_prog_attach(prp->prov->impl->prog_type, atype, fd,
				dt_tp_get_event_id(prp), dp, lvl, buf, sz);
	close(fd);

	return rc;
}

/*******************************\
 * KPROBE-based implementation *
\*******************************/

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
static int kprobe_trampoline(dt_pcb_t *pcb, uint_t exitlbl)
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

static int kprobe_attach(dtrace_hdl_t *dtp, const dt_probe_t *prp, int bpf_fd)
{
	if (!dt_tp_probe_has_info(prp)) {
		char	*fn;
		FILE	*f;
		size_t	len;
		int	fd, rc = -1;

		/*
		 * Register the kprobe with the tracing subsystem.  This will
		 * create a tracepoint event.
		 */
		fd = open(KPROBE_EVENTS, O_WRONLY | O_APPEND);
		if (fd == -1)
			return -ENOENT;

		rc = dprintf(fd, "%c:" FBT_GROUP_FMT "/%s %s\n",
			     prp->desc->prb[0] == 'e' ? 'p' : 'r',
			     FBT_GROUP_DATA, prp->desc->fun, prp->desc->fun);
		close(fd);
		if (rc == -1)
			return -ENOENT;

		/* create format file name */
		len = snprintf(NULL, 0, "%s" FBT_GROUP_FMT "/%s/format",
			       EVENTSFS, FBT_GROUP_DATA, prp->desc->fun) + 1;
		fn = dt_alloc(dtp, len);
		if (fn == NULL)
			return -ENOENT;

		snprintf(fn, len, "%s" FBT_GROUP_FMT "/%s/format", EVENTSFS,
			 FBT_GROUP_DATA, prp->desc->fun);

		/* open format file */
		f = fopen(fn, "r");
		dt_free(dtp, fn);
		if (f == NULL)
			return -ENOENT;

		/* read event id from format file */
		rc = dt_tp_probe_info(dtp, f, 0, prp, NULL, NULL);
		fclose(f);

		if (rc < 0)
			return -ENOENT;
	}

	/* attach BPF program to the probe */
	return dt_tp_probe_attach(dtp, prp, bpf_fd);
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
static void kprobe_detach(dtrace_hdl_t *dtp, const dt_probe_t *prp)
{
	int		fd;

	if (!dt_tp_probe_has_info(prp))
		return;

	dt_tp_probe_detach(dtp, prp);

	fd = open(KPROBE_EVENTS, O_WRONLY | O_APPEND);
	if (fd == -1)
		return;

	dprintf(fd, "-:" FBT_GROUP_FMT "/%s\n", FBT_GROUP_DATA,
		prp->desc->fun);
	close(fd);
}

dt_provimpl_t	dt_fbt_fprobe = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_TRACING,
	.stack_skip	= 4,
	.populate	= &populate,
	.load_prog	= &fprobe_prog_load,
	.trampoline	= &fprobe_trampoline,
	.attach		= &dt_tp_probe_attach_raw,
	.probe_info	= &fprobe_probe_info,
	.detach		= &dt_tp_probe_detach,
	.probe_destroy	= &dt_tp_probe_destroy,
};

dt_provimpl_t	dt_fbt_kprobe = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_KPROBE,
	.populate	= &populate,
	.load_prog	= &dt_bpf_prog_load,
	.trampoline	= &kprobe_trampoline,
	.attach		= &kprobe_attach,
	.detach		= &kprobe_detach,
	.probe_destroy	= &dt_tp_probe_destroy,
};
