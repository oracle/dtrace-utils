/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The Function Boundary Tracing (FBT) provider for DTrace.
 *
 * Kernnel functions can be traced through fentry/fexit probes (when available)
 * and kprobes.  The FBT provider supports both implementations and will use
 * fentry/fexit probes if the kernel supports them, and fallback to kprobes
 * otherwise.  The FBT provider does not support tracing synthetic functions
 * (i.e. compiler-generated functions with a . in their name).
 *
 * The rawfbt provider implements a variant of the FBT provider and always uses
 * kprobes.  This provider allow tracing of synthetic function.
 *
 * Mapping from event name to DTrace probe name:
 *
 *	<name>					fbt:vmlinux:<name>:entry
 *						fbt:vmlinux:<name>:return
 *						rawfbt:vmlinux:<name>:entry
 *						rawfbt:vmlinux:<name>:return
 *   or
 *	<name> [<modname>]			fbt:<modname>:<name>:entry
 *						fbt:<modname>:<name>:return
 *						rawfbt:<modname>:<name>:entry
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

static const char		prvname[] = "fbt";

#define KPROBE_EVENTS		TRACEFS "kprobe_events"

#define FBT_PROBE_FMT		"dt_%d_%s_%s"

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

dt_provimpl_t			dt_fbt_fprobe;
dt_provimpl_t			dt_fbt_kprobe;
dt_provimpl_t			dt_rawfbt;

/*
 * Create the fbt and rawfbt providers.
 */
static int populate(dtrace_hdl_t *dtp)
{
	dt_fbt = BPF_HAS(dtp, BPF_FEAT_FENTRY) ? dt_fbt_fprobe : dt_fbt_kprobe;
	dt_dprintf("fbt: Using %s implementation\n",
		   BPF_HAS(dtp, BPF_FEAT_FENTRY) ? "fentry/fexit" : "kprobe");

	if (dt_provider_create(dtp, dt_fbt.name, &dt_fbt, &pattr,
			       NULL) == NULL ||
	    dt_provider_create(dtp, dt_rawfbt.name, &dt_rawfbt, &pattr,
			       NULL) == NULL)
		return -1;			/* errno already set */

	return 0;
}

/* Create a probe (if it does not exist yet). */
static int provide_probe(dtrace_hdl_t *dtp, dt_module_t *dmp,
			 dtrace_probedesc_t *pdp)
{
	dt_provider_t	*prv = dt_provider_lookup(dtp, pdp->prv);

	if (prv == NULL)
		return 0;
	pdp->mod = dmp->dm_name;
	if (dt_probe_lookup(dtp, pdp) != NULL)
		return 0;

	/*
	 * Do not provide fentry/fexit-based probes for functions that BPF does
	 * not currently support (see dt_btf_func_is_traceable() for more
	 * details).
	 */
	if (prv->impl->prog_type == BPF_PROG_TYPE_TRACING) {
		int32_t	btf_id;

		btf_id = dt_btf_lookup_name_kind(dtp, dmp, pdp->fun, BTF_KIND_FUNC);
		if (btf_id <= 0 ||
		    !dt_btf_func_is_traceable(dtp, dmp->dm_btf, btf_id))
			return -1;
	}

	if (dt_tp_probe_insert(dtp, prv, pdp->prv, pdp->mod, pdp->fun, pdp->prb))
		return 1;

	return 0;
}

/*
 * Try to provide probes for the given probe description.  The caller ensures
 * that the provider name in probe description (if any) is a match for this
 * provider.  When this is called, we already know that this provider matches
 * the provider component of the probe specification.
 */
#define FBT_ENTRY	1
#define FBT_RETURN	2

static int provide(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp)
{
	int			n = 0;
	int			prb = 0;
	int			rawfbt = 0;
	dt_module_t		*dmp = NULL;
	dt_symbol_t		*sym = NULL;
	dt_htab_next_t		*it = NULL;
	dtrace_probedesc_t	pd;

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
	 * Unless we are dealing with a rawfbt probe, synthetic functions are
	 * not supported.
	 */
	if (strcmp(pdp->prv, dt_rawfbt.name) != 0) {
		if (strchr(pdp->fun, '.'))
			return 0;
	} else
		rawfbt = 1;

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
	 * Ensure that kernel symbols that are FBT-traceable are marked as
	 * such.  We don't do this earlier in this function so that the
	 * preceding tests have the greatest opportunity to avoid doing this
	 * unnecessarily.
	 */
	dt_modsym_mark_traceable(dtp);

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
			pd.fun = pdp->fun;

			if (prb & FBT_ENTRY) {
				pd.prb = "entry";
				n += provide_probe(dtp, dmp, &pd);
			}
			if (prb & FBT_RETURN) {
				pd.prb = "return";
				n += provide_probe(dtp, dmp, &pd);
			}

			return n;
		}

		sym = dt_symbol_by_name(dtp, pdp->fun);
		while (sym != NULL) {
			dmp = dt_symbol_module(sym);
			if (dt_symbol_traceable(sym) &&
			    dt_gmatch(dmp->dm_name, pdp->mod)) {
				pd.id = DTRACE_IDNONE;
				pd.prv = pdp->prv;
				pd.fun = pdp->fun;

				if (prb & FBT_ENTRY) {
					pd.prb = "entry";
					n += provide_probe(dtp, dmp, &pd);
				}
				if (prb & FBT_RETURN) {
					pd.prb = "return";
					n += provide_probe(dtp, dmp, &pd);
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

		/* Function name cannot be synthetic (unless rawfbt) and must match. */
		fun = dt_symbol_name(sym);
		if ((!rawfbt && strchr(fun, '.')) || !dt_gmatch(fun, pdp->fun))
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
		pd.fun = fun;

		if (prb & FBT_ENTRY) {
			pd.prb = "entry";
			n += provide_probe(dtp, smp, &pd);
		}
		if (prb & FBT_RETURN) {
			pd.prb = "return";
			n += provide_probe(dtp, smp, &pd);
		}
	}

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

		/*
		 * We want to copy entry args from %r8 to %r7 (plus offsets).
		 * Unfortunately, for fprobes, the BPF verifier can reject
		 * certain argument types.  We work around this by copying
		 * the arguments onto the BPF stack and loading them from there.
		 */
		emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_FP));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DT_TRAMP_SP_SLOT(prp->argc - 1)));
		emit(dlp, BPF_MOV_IMM(BPF_REG_2, 8 * prp->argc));
		emit(dlp, BPF_MOV_REG(BPF_REG_3, BPF_REG_8));
		emit(dlp, BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_probe_read_kernel]));

		for (i = 0; i < prp->argc; i++) {
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_FP, DT_TRAMP_SP_SLOT(prp->argc - 1) + i * 8));
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
			emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_8));
			emit(dlp, BPF_MOV_REG(BPF_REG_2, BPF_REG_FP));
			emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DT_TRAMP_SP_SLOT(0)));
			emit(dlp, BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_get_func_ret]));
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_FP, DT_TRAMP_SP_SLOT(0)));
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

	btf_id = dt_btf_lookup_name_kind(dtp, dmp, desc->fun, BTF_KIND_FUNC);
	if (btf_id <= 0)
		goto done;

	dt_tp_probe_set_id(prp, btf_id);

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
				dt_tp_probe_get_id(prp), dp, lvl, buf, sz);
	close(fd);

	return rc;
}

/*******************************\
 * KPROBE-based implementation *
\*******************************/

/*
 * Generate a BPF trampoline for a FBT (or rawfbt) probe.
 *
 * The trampoline function is called when a FBT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_(raw)fbt(dt_pt_regs *regs)
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
		 * (raw)fbt:::return arg0 should be the function offset for
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
	const char	*fun = prp->desc->fun;
	char		*tpn = (char *)fun;
	int		rc = -1;

	if (!dt_tp_probe_has_info(prp)) {
		char	*fn;
		FILE	*f;
		int	fd;

		/*
		 * For rawfbt probes, we need to apply a . -> _ conversion to
		 * ensure the tracepoint name is valid.
		 */
		if (strcmp(prp->desc->prv, dt_rawfbt.name) == 0) {
			char	*p;

			tpn = strdup(fun);
			for (p = tpn; *p; p++) {
				if (*p == '.')
					*p = '_';
			}
		}

		/*
		 * Register the kprobe with the tracing subsystem.  This will
		 * create a tracepoint event.
		 */
		fd = open(KPROBE_EVENTS, O_WRONLY | O_APPEND);
		if (fd == -1)
			goto out;

		rc = dprintf(fd, "%c:" FBT_PROBE_FMT "/%s %s\n",
			     prp->desc->prb[0] == 'e' ? 'p' : 'r',
			     PROBE_DATA, tpn, fun);
		close(fd);
		if (rc == -1)
			goto out;

		/* create format file name */
		if (asprintf(&fn, "%s" FBT_PROBE_FMT "/%s/format", EVENTSFS,
			     PROBE_DATA, tpn) == -1)
			goto out;

		/* open format file */
		f = fopen(fn, "r");
		free(fn);
		if (f == NULL)
			goto out;

		/* read event id from format file */
		rc = dt_tp_probe_info(dtp, f, NULL, prp, NULL, NULL);
		fclose(f);

		if (rc < 0)
			goto out;
	}

	/* attach BPF program to the probe */
	rc = dt_tp_probe_attach(dtp, prp, bpf_fd);

out:
	if (tpn != prp->desc->fun)
		free(tpn);

	return rc == -1 ? -ENOENT : rc;
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
static void kprobe_detach(dtrace_hdl_t *dtp, const dt_probe_t *prp)
{
	int	fd;
	char	*tpn = (char *)prp->desc->fun;

	if (!dt_tp_probe_has_info(prp))
		return;

	dt_tp_probe_detach(dtp, prp);

	fd = open(KPROBE_EVENTS, O_WRONLY | O_APPEND);
	if (fd == -1)
		return;

	/*
	 * For rawfbt probes, we need to apply a . -> _ conversion to ensure
	 * the tracepoint name is valid.
	 */
	if (strcmp(prp->desc->prv, dt_rawfbt.name) == 0) {
		char	*p;

		tpn = strdup(tpn);
		for (p = tpn; *p; p++) {
			if (*p == '.')
				*p = '_';
		}
	}

	dprintf(fd, "-:" FBT_PROBE_FMT "/%s\n", PROBE_DATA, tpn);
	close(fd);

	if (tpn != prp->desc->fun)
		free(tpn);
}

/*
 * raafbt only:
 *
 * Accept all clauses, except those that use the return() action in a function
 * that does not allow error rejection.
 */
#define FUNCS_ALLOW_RETURN	"/sys/kernel/debug/error_injection/list"

typedef struct allowed_fn {
	const char	*mod;
	const char	*fun;
	dt_hentry_t	he;
} allowed_fun_t;

static uint32_t
fun_hval(const allowed_fun_t *p)
{
	return str2hval(p->fun, p->mod ? str2hval(p->mod, 0) : 0);
}

static int
fun_cmp(const allowed_fun_t *p, const allowed_fun_t *q) {
	int	rc;

	if (p->mod != NULL) {
		if (q->mod == NULL)
			return 1;
		else {
			rc = strcmp(p->mod, q->mod);
			if (rc != 0)
				return rc;
		}
	} else if (q->mod != NULL)
		return -1;

	return strcmp(p->fun, q->fun);
}

DEFINE_HE_STD_LINK_FUNCS(fun, allowed_fun_t, he)

static void *
fun_del_entry(allowed_fun_t *head, allowed_fun_t *p)
{
	head = fun_del(head, p);

	free((char *)p->mod);
	free((char *)p->fun);
	free(p);

	return head;
}

static dt_htab_ops_t fun_htab_ops = {
	.hval = (htab_hval_fn)fun_hval,
	.cmp = (htab_cmp_fn)fun_cmp,
	.add = (htab_add_fn)fun_add,
	.del = (htab_del_fn)fun_del_entry,
	.next = (htab_next_fn)fun_next
};

static void reject_clause(const dt_probe_t *prp, int clsflags)
{
	dt_htab_t	*atab = prp->prov->prv_data;
	allowed_fun_t	tmpl;

	/* If the clause does not have a return() action, allow it. */
	if (!(clsflags & DT_CLSFLAG_RETURN))
		return;

	/*
	 * At this point, if anything fails, we reject the clause.
	 *
	 * If the htab of allowed functions for return() does not exist yet,
	 * create it.
	 */
	if (atab == NULL) {
		FILE		*f;
		char		*buf = NULL;
		size_t		len = 0;
		allowed_fun_t	*entry;

		atab = dt_htab_create(&fun_htab_ops);
		if (atab == NULL)
			goto reject;

		prp->prov->prv_data = atab;

		f = fopen(FUNCS_ALLOW_RETURN, "r");
		if (f == NULL)
			goto reject;

		while (getline(&buf, &len, f) >= 0) {
			char	*p;

			entry = (allowed_fun_t *)malloc(sizeof(allowed_fun_t));
			if (entry == NULL)
				break;

			p = strchr(buf, ' ');
			if (p == NULL)
				p = strchr(buf, '\t');

			if (p) {
				*p++ = '\0';
				if (*p == '[') {
					char	*q;

					p++;
					q = strchr(p, ']');
					if (q)
						*q = '\0';
				} else
					p = NULL;
			}

			entry->fun = strdup(buf);
			entry->mod = p != NULL ? strdup(p) : NULL;

			if (dt_htab_insert(atab, entry) < 0)
				goto reject;
		}

		free(buf);
		fclose(f);
	}

	/*
	 * If <mod>:<fun> is found, we allow the clause.
	 * If :<fun> is found, we allow the clause.
	 */
	tmpl.mod = prp->desc->mod;
	tmpl.fun = prp->desc->fun;
	if (dt_htab_lookup(atab, &tmpl) != NULL)
		return;

	tmpl.mod = NULL;
	if (dt_htab_lookup(atab, &tmpl) != NULL)
		return;

reject:
	xyerror(D_ACT_RETURN, "return() not allowed for %s:%s:%s:%s\n",
		prp->desc->prv, prp->desc->mod, prp->desc->fun, prp->desc->prb);
}

dt_provimpl_t	dt_fbt_fprobe = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_TRACING,
	.stack_skip	= 3,
	.populate	= &populate,
	.provide	= &provide,
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
	.provide	= &provide,
	.load_prog	= &dt_bpf_prog_load,
	.trampoline	= &kprobe_trampoline,
	.attach		= &kprobe_attach,
	.detach		= &kprobe_detach,
	.probe_destroy	= &dt_tp_probe_destroy,
};

dt_provimpl_t	dt_fbt = {
	.name		= prvname,
	.populate	= &populate,
};

dt_provimpl_t	dt_rawfbt = {
	.name		= "rawfbt",
	.prog_type	= BPF_PROG_TYPE_KPROBE,
	.populate	= &populate,
	.provide	= &provide,
	.load_prog	= &dt_bpf_prog_load,
	.reject_clause	= &reject_clause,
	.trampoline	= &kprobe_trampoline,
	.attach		= &kprobe_attach,
	.detach		= &kprobe_detach,
	.probe_destroy	= &dt_tp_probe_destroy,
};
