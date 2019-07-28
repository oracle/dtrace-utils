/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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
 *
 * Mapping from BPF section name to DTrace probe name:
 *
 *	tracepoint/<group>/<name>		sdt:<group>::<name>
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/bpf.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "dt_provider.h"
#include "dt_probe.h"

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

static const char		provname[] = "sdt";
static const char		modname[] = "vmlinux";

#define PROBE_LIST		TRACEFS "available_events"

#define KPROBES			"kprobes:"
#define SYSCALLS		"syscalls:"

#define FIELD_PREFIX		"field:"

#define SKIP_FIELDS_COUNT	4

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
 */
int tp_event_info(dtrace_hdl_t *dtp, FILE *f, int skip, int *idp, int *argcp,
		  dt_argdesc_t **argvp)
{
	char		buf[1024];
	int		argc;
	size_t		argsz = 0;
	dt_argdesc_t	*argv = NULL;
	char		*strp;

	*idp = -1;

	/*
	 * Pass 1:
	 * Determine the event id and the number of arguments (along with the
	 * total size of all type strings together).
	 */
	argc = -skip;
	while (fgets(buf, sizeof(buf), f)) {
		char	*p = buf;

		if (sscanf(buf, "ID: %d\n", idp) == 1)
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
	 * If there are no arguments, we are done.
	 */
	if (argc < 0)
		return -EINVAL;
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

static int sdt_probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
			  int *idp, int *argcp, dt_argdesc_t **argvp)
{
	FILE	*f;
	char   	 fn[256];
	int	rc;

	*idp = -1;

	strcpy(fn, EVENTSFS);
	strcat(fn, prp->desc->mod);
	strcat(fn, "/");
	strcat(fn, prp->desc->prb);
	strcat(fn, "/format");

	f = fopen(fn, "r");
	if (!f)
		return -ENOENT;

	rc = tp_event_info(dtp, f, SKIP_FIELDS_COUNT, idp, argcp, argvp);
	fclose(f);

	return rc;
}

/*
 * The PROBE_LIST file lists all tracepoints in a <group>:<name> format.  When
 * kprobes are registered on the system, they will appear in this list also as
 * kprobes:<name>.  We need to ignore them because DTrace already accounts for
 * them as FBT probes.
 */
static int sdt_populate(dtrace_hdl_t *dtp)
{
	dt_provider_t	*prv;
	FILE		*f;
	char		buf[256];
	char		*p;
	int		n = 0;

	if (!(prv = dt_provider_create(dtp, "sdt", &dt_sdt, &pattr)))
		return 0;

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return 0;

	while (fgets(buf, sizeof(buf), f)) {
		p = strchr(buf, '\n');
		if (p)
			*p = '\0';

		p = strchr(buf, ':');
		if (p == NULL) {
			if (dt_probe_insert(dtp, prv, provname, modname, "",
					    buf))
				n++;
		} else if (memcmp(buf, KPROBES, sizeof(KPROBES) - 1) == 0) {
			continue;
		} else if (memcmp(buf, SYSCALLS, sizeof(SYSCALLS) - 1) == 0) {
			continue;
		} else {
			*p++ = '\0';

			if (dt_probe_insert(dtp, prv, provname, buf, "", p))
				n++;
		}
	}

	fclose(f);

	return n;
}

#if 0
#define EVENT_PREFIX	"tracepoint/"
#define EVENT_SYSCALLS	"syscalls/"
#define EVENT_KPROBES	"kprobes/"

/*
 * Perform a probe lookup based on an event name (usually obtained from a BPF
 * ELF section name).  Exclude syscalls and kprobes tracepoint events because
 * they are handled by their own individual providers.
 */
static struct dt_probe *sdt_resolve_event(const char *name)
{
	char		*str, *p;
	struct dt_probe	tmpl;
	struct dt_probe	*probe;

	if (!name)
		return NULL;

	/* Exclude anything that is not a tracepoint */
	if (strncmp(name, EVENT_PREFIX, sizeof(EVENT_PREFIX) - 1) != 0)
		return NULL;
	name += sizeof(EVENT_PREFIX) - 1;

	/* Exclude syscall tracepoints */
	if (strncmp(name, EVENT_SYSCALLS, sizeof(EVENT_SYSCALLS) - 1) == 0)
		return NULL;

	/* Exclude kprobes tracepoints */
	if (strncmp(name, EVENT_KPROBES, sizeof(EVENT_KPROBES) - 1) == 0)
		return NULL;

	str = strdup(name);
	if (!str)
		return NULL;

	p = strchr(str, '/');
	*p++ = '\0';

	memset(&tmpl, 0, sizeof(tmpl));
	tmpl.prv_name = provname;
	tmpl.mod_name = p ? str : modname;
	tmpl.fun_name = NULL;
	tmpl.prb_name = p;

	probe = dt_probe_by_name(&tmpl);

	free(str);

	return probe;
}

static int sdt_attach(const char *name, int bpf_fd)
{
	char	efn[256];
	int	len = 265 - strlen(EVENTSFS);

	name += 11;				/* skip "tracepoint/" */
	strcpy(efn, EVENTSFS);
	strncat(efn, name, len);
	len -= strlen(name);
	strncat(efn, "/id", len);
printf("[%s]\n", efn);

	return 0;
}
#endif

dt_provimpl_t	dt_sdt = {
	.name		= "sdt",
	.populate	= &sdt_populate,
	.probe_info	= &sdt_probe_info,
#if 0
	.resolve_event	= &sdt_resolve_event,
	.attach		= &sdt_attach,
#endif
};
