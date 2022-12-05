/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Provider support code for tracepoint-based probes.
 */
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>

#include "dt_bpf.h"
#include "dt_provider.h"
#include "dt_probe.h"
#include "dt_impl.h"

/*
 * All tracing events (tracepoints) include a number of fields that we need to
 * skip in the tracepoint format description.  These fields are: common_type,
 * common_flags, common_preempt_coint, and common_pid.
 */
#define SKIP_FIELDS_COUNT	4

/*
 * Tracepoint-specific probe data.  This is allocated for every tracepoint
 * based probe.
 */
struct tp_probe {
	int	event_id;		/* tracepoint event id */
	int	event_fd;		/* tracepoint perf event fd */
};

/*
 * Allocate tracepoint-specific probe data.
 */
tp_probe_t *
dt_tp_alloc(dtrace_hdl_t *dtp)
{
	tp_probe_t	*tpp;

	tpp = dt_zalloc(dtp, sizeof(tp_probe_t));
	if (tpp == NULL)
		return NULL;

	tpp->event_id = -1;
	tpp->event_fd = -1;

	return tpp;
}

/*
 * Attach the given (loaded) BPF program to the given tracepoint probe.  This
 * function performs the necessary steps for attaching the BPF program to a
 * tracepoint based probe by opening a perf event for the tracepoint, and
 * associating the BPF program with the perf event.
 */
int
dt_tp_attach(dtrace_hdl_t *dtp, tp_probe_t *tpp, int bpf_fd)
{
	if (tpp->event_id == -1)
		return 0;

	if (tpp->event_fd == -1) {
		int			fd;
		struct perf_event_attr	attr = { 0, };

		attr.type = PERF_TYPE_TRACEPOINT;
		attr.size = sizeof(attr);
		attr.sample_period = 1;
		attr.wakeup_events = 1;
		attr.config = tpp->event_id;

		fd = perf_event_open(&attr, -1, 0, -1, 0);
		if (fd < 0)
			return -errno;

		tpp->event_fd = fd;
	}

	if (ioctl(tpp->event_fd, PERF_EVENT_IOC_SET_BPF, bpf_fd) < 0)
		return -errno;

	return 0;
}

/*
 * Return whether the tracepoint has already been created at the kernel level.
 */
int
dt_tp_is_created(const tp_probe_t *tpp)
{
	return tpp->event_id != -1;
}

/*
 * Parse a EVENTSFS/<group>/<event>/format file to determine the event id and
 * the argument types.
 *
 * The event id is easy enough to parse out, because it appears on a line in
 * the following format:
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
 * specify an additional number of fields to skip (using the 'skip' parameter)
 * before we get to the actual arguments.
 */
int
dt_tp_event_info(dtrace_hdl_t *dtp, FILE *f, int skip, tp_probe_t *tpp,
		 int *argcp, dt_argdesc_t **argvp)
{
	char		*buf = NULL;
	size_t		bufsz;
	int		argc;
	size_t		argsz = 0;
	dt_argdesc_t	*argv = NULL;
	char		*strp;

	tpp->event_id = -1;

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
	while (getline(&buf, &bufsz, f) >= 0) {
		char	*p = buf;

		if (sscanf(buf, "ID: %d\n", &tpp->event_id) == 1)
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
	free(buf);
	buf = NULL;

	/*
	 * If we saw less fields than expected, we flag an error.
	 * If we are not interested in arguments, we are done.
	 * If there are no arguments, we are done.
	 */
	if (argc < 0)
		return -EINVAL;
	if (argcp == NULL || argvp == NULL)
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
	while (getline(&buf, &bufsz, f) >= 0) {
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
	free(buf);
	*argcp = argc;
	*argvp = argv;

	return 0;
}

/*
 * Detach from a tracepoint for a tracepoint-based probe.  The caller should
 * still call dt_tp_destroy() to free the tracepoint-specific probe data.
 */
void
dt_tp_detach(dtrace_hdl_t *dtp, tp_probe_t *tpp)
{
	tpp->event_id = -1;

	if (tpp->event_fd != -1) {
		close(tpp->event_fd);
		tpp->event_fd = -1;
	}
}

/*
 * Clean up the tracepoint-specific data for a probe.  This may be called with
 * tracepoint-specific data that has not been attached to a probe (e.g. if the
 * creation of the actual probe failed).
 */
void
dt_tp_destroy(dtrace_hdl_t *dtp, tp_probe_t *tpp)
{
	dt_free(dtp, tpp);
}

/*
 * Create a tracepoint-based probe.  This function is called from any provider
 * that handles tracepoint-based probes.  It allocates tracepoint-specific data
 * for the probe, and adds the probe to the framework.
 */
dt_probe_t *
dt_tp_probe_insert(dtrace_hdl_t *dtp, dt_provider_t *prov, const char *prv,
		   const char *mod, const char *fun, const char *prb)
{
	tp_probe_t	*tpp;

	tpp = dt_tp_alloc(dtp);
	if (tpp == NULL)
		return NULL;

	return dt_probe_insert(dtp, prov, prv, mod, fun, prb, tpp);
}

/*
 * Convenience function for basic tracepoint-based probe attach.
 */
int
dt_tp_probe_attach(dtrace_hdl_t *dtp, const dt_probe_t *prp, int bpf_fd)
{
	return dt_tp_attach(dtp, prp->prv_data, bpf_fd);
}

/*
 * Convenience function for basic tracepoint-based probe detach.
 */
void
dt_tp_probe_detach(dtrace_hdl_t *dtp, const dt_probe_t *prp)
{
	dt_tp_detach(dtp, prp->prv_data);
}

/*
 * Convenience function for probe cleanup for tracepoint-based probes.
 */
void
dt_tp_probe_destroy(dtrace_hdl_t *dtp, void *datap)
{
	dt_tp_destroy(dtp, datap);
}
