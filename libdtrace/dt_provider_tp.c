/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Provider support code for tracepoint-based probes.
 */
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>

#include "dt_bpf.h"
#include "dt_provider_tp.h"
#include "dt_probe.h"
#include "dt_impl.h"

/*
 * Tracepoint-specific probe data.  This is allocated for every tracepoint
 * based probe.  Since 0 is not a valid tracepoint event id, and given that BTF
 * id 0 refers to 'void', this value is used to denote that no association with
 * a tracepoint or BTF type exists yet.
 */
struct tp_probe {
	uint32_t	id;	/* tracepoint event id or BTF id */
	int		fd;	/* tracepoint perf event fd */
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

	tpp->id = 0;
	tpp->fd = -1;

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
	if (tpp->id == 0)
		return 0;

	if (tpp->fd == -1) {
		int			fd;
		struct perf_event_attr	attr = { 0, };

		attr.type = PERF_TYPE_TRACEPOINT;
		attr.size = sizeof(attr);
		attr.sample_period = 1;
		attr.wakeup_events = 1;
		attr.config = tpp->id;

		fd = dt_perf_event_open(&attr, -1, 0, -1, 0);
		if (fd < 0)
			return fd;

		tpp->fd = fd;
	}

	if (ioctl(tpp->fd, PERF_EVENT_IOC_SET_BPF, bpf_fd) < 0)
		return -errno;

	return 0;
}

/*
 * Attach the given (loaded) BPF program to the given raw tracepoint.  The
 * raw tracepoint is identified by name (tpp->id is UINT_MAX) or BTF id
 * (tpp->id).
 */
int
dt_tp_attach_raw(dtrace_hdl_t *dtp, tp_probe_t *tpp, const char *name,
		 int bpf_fd)
{
	if (tpp->fd == -1) {
		int	fd;

		fd = dt_bpf_raw_tracepoint_open(
			tpp->id == UINT_MAX ? name : NULL,
			bpf_fd);
		if (fd < 0)
			return -errno;

		tpp->fd = fd;
	}

	return 0;
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
 * All events include a number of common fields that we are not interested
 * in and that need to be skipped.  Callers of this function can specify an
 * additional number of fields to skip (using the 'skip' parameter) before
 * we get to the actual arguments.
 */
int
dt_tp_event_info(dtrace_hdl_t *dtp, FILE *f, int skip, tp_probe_t *tpp,
		 int *argcp, dt_argdesc_t **argvp)
{
	char		*buf = NULL;
	size_t		bufsz;
	int		argc, common = 1;
	dt_argdesc_t	*argv = NULL;

	tpp->id = 0;

	/*
	 * Pass 1:
	 * Determine the event id and the number of arguments.
	 * Skip over how ever many arguments the caller asks us to skip.
	 * We will skip initial "common fields" as well.
	 */
	argc = -skip;
	while (getline(&buf, &bufsz, f) >= 0) {
		char	*p = buf;

		if (sscanf(buf, "ID: %u\n", &tpp->id) == 1)
			continue;
		if (sscanf(buf, " field:%[^;]", p) <= 0)
			continue;

		/*
		 * If we have only seen common fields to date, keep
		 * looking for a non-common field.
		 */
		if (common == 1) {
			char	*s = p + strlen(p) - 1;

			/*
			 * Strip off any [] array size specifications at the end.
			 */
			while (*s == ']') {
				/* From ']' hunt back to '['.  They are not nested. */
				while (s > p && *(--s) != '[') ;

				/* Then remove any spaces. */
				while (s > p && *(--s) == ' ') ;
			}
			*(++s) = '\0';

			/*
			 * Go to the beginning of the identifier.
			 */
			p = strrchr(p, ' ');
			if (p == NULL)
				return -EINVAL;
			p++;

			/*
			 * Check if it is a common field.
			 *
			 * In kernel source file kernel/trace/trace_events.c
			 * in trace_define_common_fields(), the macro
			 * __common_field() is used to define common fields,
			 * prepending names with "common_".
			 */
			if (strncmp(p, "common_", 7) == 0) {
				/*
				 * For Pass 2, we will not bother checking
				 * for "common" fields;  we will just pretend
				 * the caller asked us to skip more arguments.
				 */
				skip++;

				continue;
			}

			/*
			 * No more need to check for common fields.
			 */
			common = 0;
		}

		/* We found a non-common "field:" description. */
		argc++;
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

	argv = dt_calloc(dtp, argc, sizeof(dt_argdesc_t));
	if (!argv)
		return -ENOMEM;

	/*
	 * Pass 2:
	 * Fill in the actual argument datatype strings.
	 */
	rewind(f);
	argc = -skip;
	while (getline(&buf, &bufsz, f) >= 0) {
		char	*p;
		size_t	l;
		size_t	size = 0;
		char	tstr[DT_TYPE_NAMELEN];
		char	*strp;

		p = strstr(buf, "size:");
		if (p != NULL)
			size = strtol(p + 5, NULL, 10);

		p = buf;
		if (sscanf(buf, " field:%[^;]", p) <= 0)
			continue;

		/* We found a field: description - see if we should skip it. */
		if (argc < 0)
			goto skip;

		sscanf(p, "__data_loc %[^;]", p);

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
			strp = p;
		} else {
			char	*s, *q;
			int	alpha = 0;

			/*
			 * The identifier is followed by at least one array
			 * size specification.  Find the beginning of the
			 * sequence of (one or more) array size specifications.
			 * We also skip any spaces in front of [ characters.
			 */
			s = p + l - 1;
			for (;;) {
				while (*(--s) != '[') {
					if (!isdigit(*s))
						alpha = 1;
				}
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

			strp = tstr;
			if (alpha) {
				ctf_file_t	*ctfp = dtp->dt_shared_ctf;
				ctf_id_t	type;
				size_t		esize = 1;

				type = ctf_lookup_by_name(ctfp, p);
				if (type != CTF_ERR)
					esize = ctf_type_size(ctfp, type);
				if (esize != 0)
					size /= esize;

				l = snprintf(strp, q - p + strlen(s), "%s[%lu]",
					     p, size);
			} else {
				l = q - p;
				memcpy(strp, p, l);
				memcpy(strp + l, s, strlen(s) + 1);
			}
		}

		argv[argc].mapping = argc;
		argv[argc].flags = 0;
		argv[argc].native = strdup(strp);
		argv[argc].xlate = NULL;

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
 * Return whether event info for a tracepoint is valid.
 */
int
dt_tp_has_info(const tp_probe_t *tpp)
{
	return tpp->id > 0;
}

/*
 * Detach from a tracepoint for a tracepoint-based probe.  The caller should
 * still call dt_tp_destroy() to free the tracepoint-specific probe data.
 */
void
dt_tp_detach(dtrace_hdl_t *dtp, tp_probe_t *tpp)
{
	tpp->id = 0;

	if (tpp->fd != -1) {
		close(tpp->fd);
		tpp->fd = -1;
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
 * Return the (event or BTF) id for the tracepoint.
 */
uint32_t
dt_tp_get_id(const tp_probe_t *tpp)
{
	return tpp->id;
}

/*
 * Set the (event or BTF) id for the tracepoint.
 */
void
dt_tp_set_id(tp_probe_t *tpp, uint32_t id)
{
	tpp->id = id;
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
 * Parse a EVENTSFS/<group>/<event>/format file to determine the event id and
 * the argument types for a given probe.
 */
int
dt_tp_probe_info(dtrace_hdl_t *dtp, FILE *f, int skip, const dt_probe_t *prp,
		 int *argcp, dt_argdesc_t **argvp)
{
	tp_probe_t	*tpp = prp->prv_data;

	return dt_tp_event_info(dtp, f, skip, tpp, argcp, argvp);
}

/*
 * Return whether a tracepoint has been associated with this probe.
 */
int
dt_tp_probe_has_info(const dt_probe_t *prp)
{
	tp_probe_t	*tpp = prp->prv_data;

	return dt_tp_has_info(tpp);
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
 * Convenience function for raw tracepoint-based probe attach.
 */
int
dt_tp_probe_attach_raw(dtrace_hdl_t *dtp, const dt_probe_t *prp, int bpf_fd)
{
	return dt_tp_attach_raw(dtp, prp->prv_data, prp->desc->prb, bpf_fd);
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

/*
 * Convenience function to get the (event or BTF) id for the tracepoint of a
 * probe.
 */
uint32_t
dt_tp_probe_get_id(const dt_probe_t *prp)
{
	return dt_tp_get_id(prp->prv_data);
}

/*
 * Convenience function to set the (event or BTF) id for the tracepoint of a
 * probe.
 */
void
dt_tp_probe_set_id(const dt_probe_t *prp, uint32_t id)
{
	dt_tp_set_id(prp->prv_data, id);
}
