/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates. All rights reserved.
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
#include "dt_provider_tp.h"
#include "dt_probe.h"
#include "dt_pt_regs.h"

static const char		prvname[] = "sdt";
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

	prv = dt_provider_create(dtp, prvname, &dt_sdt, &pattr, NULL);
	if (prv == NULL)
		return -1;			/* errno already set */

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
 *	int dt_sdt(void *data)
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
	ssize_t		off;

	dt_cg_tramp_prologue(pcb);

	/*
	 * After the dt_cg_tramp_prologue() call, we have:
	 *				//     (%r7 = dctx->mst)
	 *				//     (%r8 = dctx->ctx)
	 */

	dt_cg_tramp_clear_regs(pcb);

	/*
	 * Decode the arguments of the underlying tracepoint (from the context)
	 * and store them in the cached argument list in the mstate.
	 *
	 * Skip the first argument (a private pointer that we are not allowed
	 * to access from BPF).
	 */
	off = sizeof(uint64_t);
	for (i = 0; i < prp->argc; i++) {
		dt_node_t	*anp = prp->xargv[i];
		ssize_t		size = ctf_type_size(anp->dn_ctfp,
						     anp->dn_type);
		ssize_t		align = ctf_type_align(anp->dn_ctfp,
						       anp->dn_type);

		off = P2ROUNDUP(off, align);
		if (dt_node_is_scalar(anp)) {
			uint_t	ldsz = dt_cg_ldsize(anp, anp->dn_ctfp,
						    anp->dn_type, NULL);

			emit(dlp, BPF_LOAD(ldsz, BPF_REG_0, BPF_REG_8, off));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(i), BPF_REG_0));
		} else {
			emit(dlp, BPF_MOV_REG(BPF_REG_0, BPF_REG_8));
			emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, off));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(i), BPF_REG_0));
		}
		off += size;
	}

	/*
	 * Clear the remainder of the cached arguments.
	 */
	for (; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++)
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(i), 0));

	dt_cg_tramp_epilogue(pcb);

	return 0;
}

/*
 * If there is no trace_event_raw_* struct available in CTF, we can still get
 * argument count and type information from the tracefs data.
 */
static int probe_info_tracefs(dtrace_hdl_t *dtp, const dt_probe_t *prp,
			      int *argcp, dt_argdesc_t **argvp)
{
	FILE				*f;
	char				*fn;
	int				rc;
	tp_probe_t			*tpp = prp->prv_data;
	const dtrace_probedesc_t	*pdp = prp->desc;

	if (asprintf(&fn, EVENTSFS "%s/%s/format", pdp->mod, pdp->prb) == -1)
		return dt_set_errno(dtp, EDT_NOMEM);

	f = fopen(fn, "r");
	free(fn);
	if (!f)
		return -ENOENT;

	rc = dt_tp_event_info(dtp, f, 0, tpp, argcp, argvp);
	fclose(f);

	return rc;
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *argcp, dt_argdesc_t **argvp)
{
#ifdef HAVE_LIBCTF
	int			rc, i;
	char			*str;
	ctf_dict_t		*ctfp;
	ctf_id_t		type;
	ctf_next_t		*it = NULL;
	int			argc = 0;
	dt_argdesc_t		*argv = NULL;
	dtrace_typeinfo_t	sym;
	FILE			*f;
	uint32_t		id;

	/* Retrieve the event id. */
	if (asprintf(&str, EVENTSFS "%s/%s/id", prp->desc->mod, prp->desc->prb) == -1)
		return dt_set_errno(dtp, EDT_NOMEM);

	f = fopen(str, "r");
	free(str);
	if (!f)
		return dt_set_errno(dtp, EDT_ENABLING_ERR);

	rc = fscanf(f, "%u", &id);
	fclose(f);

	if (rc < 0 || id == 0)
		return dt_set_errno(dtp, EDT_ENABLING_ERR);

	dt_tp_set_event_id(prp, id);

	if (asprintf(&str, "struct trace_event_raw_%s", prp->desc->prb) == -1)
		return dt_set_errno(dtp, EDT_NOMEM);
	rc = dtrace_lookup_by_type(dtp, DTRACE_OBJ_EVERY, str, &sym);
	free(str);
	if (rc ||
	    ctf_type_kind(sym.dtt_ctfp, sym.dtt_type) != CTF_K_STRUCT)
		goto use_alt;

	/*
	 * Tracepoints have an extra member at the beginning and end of the
	 * struct.  We need to skip those.  (We also handle the case where one
	 * or both of those members are missing even though that is not
	 * supposed to happen.)
	 */
	ctfp = sym.dtt_ctfp;
	type = sym.dtt_type;
	rc = ctf_member_count(ctfp, type);
	if (rc <= 2)
		goto done;

	rc--;
	argc = rc - 1;
	argv = dt_zalloc(dtp, argc * sizeof(dt_argdesc_t));
	if (argv == NULL)
		return dt_set_errno(dtp, EDT_NOMEM);
	/* Skip first member. */
	ctf_member_next(ctfp, type, &it, NULL, NULL, 0);
	for (i = 0; i < argc; i++) {
		ctf_id_t	mtyp;
		char		n[DT_TYPE_NAMELEN];

		if (ctf_member_next(ctfp, type, &it, NULL, &mtyp, 0) == CTF_ERR) {
			ctf_next_destroy(it);
			return dt_set_errno(dtp, EDT_CTF);
		}
		ctf_type_name(ctfp, mtyp, n, sizeof(n));
		argv[i].mapping = i;
		argv[i].native = strdup(n);
		argv[i].xlate = NULL;
	}
	ctf_next_destroy(it);
done:
	*argcp = argc;
	*argvp = argv;

	return 0;

use_alt:
#endif
	return probe_info_tracefs(dtp, prp, argcp, argvp);
}

dt_provimpl_t	dt_sdt = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_TRACEPOINT,
	.populate	= &populate,
	.load_prog	= &dt_bpf_prog_load,
	.trampoline	= &trampoline,
	.attach		= &dt_tp_probe_attach,
	.probe_info	= &probe_info,
	.detach		= &dt_tp_probe_detach,
	.probe_destroy	= &dt_tp_probe_destroy,
};
