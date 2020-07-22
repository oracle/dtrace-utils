/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
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

#include "dt_impl.h"
#include "dt_bpf.h"
#include "dt_bpf_builtins.h"
#include "dt_provider.h"
#include "dt_probe.h"
#include "dt_pt_regs.h"

static const char		prvname[] = "sdt";
static const char		modname[] = "vmlinux";

#define PROBE_LIST		TRACEFS "available_events"

#define KPROBES			"kprobes"
#define SYSCALLS		"syscalls"
#define UPROBES			"uprobes"

/*
 * All tracing events (tracepoints) include a number of fields that we need to
 * skip in the tracepoint format description.  These fields are: common_type,
 * common_flags, common_preempt_coint, and common_pid.
 */
#define SKIP_FIELDS_COUNT	4

#define FIELD_PREFIX		"field:"

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

/*
 * Attach the given (loaded) BPF program to the given probe.  This function
 * performs the necessary steps for attaching the BPF program to a tracepoint
 * based probe by opening a perf event for the probe, and associating the BPF
 * program with the perf event.
 */
int tp_attach(dtrace_hdl_t *dtp, const dt_probe_t *prp, int bpf_fd)
{
	tp_probe_t	*datap = prp->prv_data;

	if (datap->event_id == -1)
		return 0;

	if (datap->event_fd == -1) {
		int			fd;
		struct perf_event_attr	attr = { 0, };

		attr.type = PERF_TYPE_TRACEPOINT;
		attr.sample_type = PERF_SAMPLE_RAW;
		attr.sample_period = 1;
		attr.wakeup_events = 1;
		attr.config = datap->event_id;

		fd = perf_event_open(&attr, -1, 0, -1, 0);
		if (fd < 0)
			return dt_set_errno(dtp, errno);

		datap->event_fd = fd;
	}

	if (ioctl(datap->event_fd, PERF_EVENT_IOC_SET_BPF, bpf_fd) < 0)
		return dt_set_errno(dtp, errno);

	return 0;
}

/*
 * Create a tracepoint-based probe.  This function is called from any provider
 * that handled tracepoint-based probes.  It sets up the provider-specific
 * data of the probe, and calls dt_probe_insert() to add the probe to the
 * framework.
 */
dt_probe_t *tp_probe_insert(dtrace_hdl_t *dtp, dt_provider_t *prov,
			   const char *prv, const char *mod,
			   const char *fun, const char *prb)
{
	tp_probe_t	*datap;

	datap = dt_zalloc(dtp, sizeof(tp_probe_t));
	if (datap == NULL)
		return NULL;

	datap->event_id = -1;
	datap->event_fd = -1;

	return dt_probe_insert(dtp, prov, prv, mod, fun, prb, datap);
}

/*
 * Parse the EVENTSFS/<group>/<event>/format file to determine the event id and
 * the argument types.
 *
 * The event id easy enough to parse out, because it appears on a line in the
 * following format:
 *	ID: <num>
 *
 * The argument types are a bit more complicated.  The basic format for each
 * argument is:
 *	field:<var-decl>; offset:<num> size:<num> signed:(0|1);
 * The <var-decl> may be prefixed by __data_loc, which is a tag that we can
 * ignore.  The <var-decl> does contain an identifier name that dtrace cannot
 * cope with because it expects just the type specification.  Getting rid of
 * the identifier isn't as easy because it may be suffixed by one or more
 * array dimension specifiers (and those are part of the type).
 *
 * All events include a number of fields that we are not interested and that
 * need to be skipped (SKIP_FIELDS_COUNT).  Callers of this function can
 * specify an additional numbe of fields to skip (using the 'skip' parameter)
 * before we get to the actual arguments.
 */
int tp_event_info(dtrace_hdl_t *dtp, FILE *f, int skip, tp_probe_t *datap,
		  int *argcp, dt_argdesc_t **argvp)
{
	char		buf[1024];
	int		argc;
	size_t		argsz = 0;
	dt_argdesc_t	*argv = NULL;
	char		*strp;

	datap->event_id = -1;

	/*
	 * Let skip be the total number of fields to skip.
	 */
	skip += SKIP_FIELDS_COUNT;

	/*
	 * Pass 1:
	 * Determine the event id and the number of arguments (along with the
	 * total size of all type strings together).
	 */
	argc = -skip;
	while (fgets(buf, sizeof(buf), f)) {
		char	*p = buf;

		if (sscanf(buf, "ID: %d\n", &datap->event_id) == 1)
			continue;

		if (sscanf(buf, " field:%[^;]", p) <= 0)
			continue;
		sscanf(p, "__data_loc %[^;]", p);

		/* We found a field: description - see if we should skip it. */
		if (argc++ < 0)
			continue;

		/*
		 * We over-estimate the space needed (pass 2 will strip off the
		 * identifier name).
		 */
		argsz += strlen(p) + 1;
	}

	/*
	 * If we saw less fields than expected, we flag an error.
	 * If we are not interested in arguments, we are done.
	 * If there are no arguments, we are done.
	 */
	if (argc < 0)
		return -EINVAL;
	if (argcp == NULL)
		return 0;
	if (argc == 0)
		goto done;

	argv = dt_zalloc(dtp, argc * sizeof(dt_argdesc_t) + argsz);
	if (!argv)
		return -ENOMEM;
	strp = (char *)(argv + argc);

	/*
	 * Pass 2:
	 * Fill in the actual argument datatype strings.
	 */
	rewind(f);
	argc = -skip;
	while (fgets(buf, sizeof(buf), f)) {
		char	*p = buf;
		size_t	l;

		if (sscanf(buf, " field:%[^;]", p) <= 0)
			continue;
		sscanf(p, "__data_loc %[^;]", p);

		/* We found a field: description - see if we should skip it. */
		if (argc < 0)
			goto skip;

		/*
		 * If the last character is not ']', the last token must be the
		 * identifier name.  Get rid of it.
		 */
		l = strlen(p);
		if (p[l - 1] != ']') {
			char	*q;

			if ((q = strrchr(p, ' ')))
				*q = '\0';

			l = q - p;
			memcpy(strp, p, l);
		} else {
			char	*s, *q;
			int	n;

			/*
			 * The identifier is followed by at least one array
			 * size specification.  Find the beginning of the
			 * sequence of (one or more) array size specifications.
			 * We also skip any spaces in front of [ characters.
			 */
			s = p + l - 1;
			for (;;) {
				while (*(--s) != '[') ;
				while (*(--s) == ' ') ;
				if (*s != ']')
					break;
			}

			/*
			 * Insert a \0 byte right before the array size
			 * specifications.  The \0 byte overwrites the last
			 * character of the identifier which is fine because we
			 * know that the identifier is at least one character
			 * long.
			 */
			*(s++) = '\0';
			if ((q = strrchr(p, ' ')))
				*q = '\0';

			l = q - p;
			memcpy(strp, p, l);
			n = strlen(s);
			memcpy(strp + l, s, n);
			l += n;
		}

		argv[argc].mapping = argc;
		argv[argc].native = strp;
		argv[argc].xlate = NULL;

		strp += l + 1;

skip:
		argc++;
	}

done:
	*argcp = argc;
	*argvp = argv;

	return 0;
}

/*
 * Clean up the provider-specific data for a probe.  This may be called with
 * provider-specific data that has not been attached to a probe (e.g. if the
 * creation of the actual probe failed).
 */
void tp_probe_destroy(dtrace_hdl_t *dtp, void *datap)
{
	dt_free(dtp, datap);
}

/*
 * Perform any provider specific cleanup for a probe that is being removed from
 * the framework.
 */
void tp_probe_fini(dtrace_hdl_t *dtp, const dt_probe_t *prp)
{
	tp_probe_t	*datap = prp->prv_data;

	if (datap->event_fd != -1) {
		close(datap->event_fd);
		datap->event_fd = -1;
	}

	datap->event_id = -1;

	tp_probe_destroy(dtp, datap);
}

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

			if (tp_probe_insert(dtp, prv, prvname, buf, "", p))
				n++;
		} else {
			if (tp_probe_insert(dtp, prv, prvname, modname, "",
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
	 *	for (i = 0; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++)
	 *		dctx->mst->argv[i] = 0
	 *				// stdw [%r7 + DCTX_ARG(0), 0
	 */
	for (i = 0; i < pcb->pcb_pinfo.dtp_argc; i++) {
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

	strcpy(fn, EVENTSFS);
	strcat(fn, prp->desc->mod);
	strcat(fn, "/");
	strcat(fn, prp->desc->prb);
	strcat(fn, "/format");

	f = fopen(fn, "r");
	if (!f)
		return -ENOENT;

	rc = tp_event_info(dtp, f, 0, datap, argcp, argvp);
	fclose(f);

	return rc;
}

dt_provimpl_t	dt_sdt = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_TRACEPOINT,
	.populate	= &populate,
	.trampoline	= &trampoline,
	.attach		= &tp_attach,
	.probe_info	= &probe_info,
	.probe_destroy	= &tp_probe_destroy,
	.probe_fini	= &tp_probe_fini,
};
