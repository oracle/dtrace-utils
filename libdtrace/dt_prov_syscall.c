/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The syscall provider for DTrace.
 *
 * System call probes are exposed by the kernel as tracepoint events in the
 * "syscalls" group.  Entry probe names start with "sys_enter_" and exit probes
 * start with "sys_exit_".
 *
 * Mapping from event name to DTrace probe name:
 *
 *	syscalls:sys_enter_<name>		syscall:vmlinux:<name>:entry
 *	syscalls:sys_exit_<name>		syscall:vmlinux:<name>:return
 *
 * Mapping from BPF section name to DTrace probe name:
 *
 *	tracepoint/syscalls/sys_enter_<name>	syscall:vmlinux:<name>:entry
 *	tracepoint/syscalls/sys_exit_<name>	syscall:vmlinux:<name>:return
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
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
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

static const char		provname[] = "syscall";
static const char		modname[] = "vmlinux";

#define SYSCALLSFS		EVENTSFS "syscalls/"

#define FIELD_PREFIX		"field:"

#define SKIP_FIELDS_COUNT	5

static int syscall_probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
			      int *idp, int *argcp, dt_argdesc_t **argvp)
{
	FILE	*f;
	char   	 fn[256];
	int	rc;

	*idp = -1;

	/*
	 * We know that the probe name is either "entry" or "return", so we can
	 * just check the first character.
	 */
	strcpy(fn, SYSCALLSFS);
	if (prp->desc->prb[0] == 'e')
		strcat(fn, "sys_enter_");
	else
		strcat(fn, "sys_exit_");
	strcat(fn, prp->desc->fun);
	strcat(fn, "/format");

	f = fopen(fn, "r");
	if (!f)
		return -ENOENT;

	rc = tp_event_info(dtp, f, SKIP_FIELDS_COUNT, idp, argcp, argvp);
	fclose(f);

	return rc;
}

#define PROBE_LIST	TRACEFS "available_events"

#define PROV_PREFIX	"syscalls:"
#define ENTRY_PREFIX	"sys_enter_"
#define EXIT_PREFIX	"sys_exit_"

/* can the PROBE_LIST file and add probes for any syscalls events. */
static int syscall_populate(dtrace_hdl_t *dtp)
{
	dt_provider_t	*prv;
	FILE		*f;
	char		buf[256];
	int		n = 0;

	if (!(prv = dt_provider_create(dtp, "syscall", &dt_syscall, &pattr)))
		return 0;

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return 0;

	while (fgets(buf, sizeof(buf), f)) {
		char	*p;

		/* Here buf is "group:event".  */
		p = strchr(buf, '\n');
		if (p)
			*p = '\0';
		else {
			/*
			 * If we didn't see a newline, the line was too long.
			 * Report it, and skip until the end of the line.
			 */
			fprintf(stderr, "%s: Line too long: %s\n",
				PROBE_LIST, buf);
			do
				fgets(buf, sizeof(buf), f);
			while (strchr(buf, '\n') == NULL);
			continue;
		}

		/* We need "group:" to match "syscalls:". */
		p = buf;
		if (memcmp(p, PROV_PREFIX, sizeof(PROV_PREFIX) - 1) != 0)
			continue;

		p += sizeof(PROV_PREFIX) - 1;
		/*
		 * Now p will be just "event", and we are only interested in
		 * events that match "sys_enter_*" or "sys_exit_*".
		 */
		if (!memcmp(p, ENTRY_PREFIX, sizeof(ENTRY_PREFIX) - 1)) {
			p += sizeof(ENTRY_PREFIX) - 1;
			if (dt_probe_insert(dtp, prv, provname, modname, p,
					    "entry"))
				n++;
		} else if (!memcmp(p, EXIT_PREFIX, sizeof(EXIT_PREFIX) - 1)) {
			p += sizeof(EXIT_PREFIX) - 1;
			if (dt_probe_insert(dtp, prv, provname, modname, p,
					    "return"))
				n++;
		}
	}

	fclose(f);

	return n;
}

#if 0
#define EVENT_PREFIX	"tracepoint/syscalls/"

/*
 * Perform a probe lookup based on an event name (BPF ELF section name).
 * Exclude syscalls and kprobes tracepoint events because they are handled by
 * their own individual providers.
 */
static struct dt_probe *systrace_resolve_event(const char *name)
{
	const char	*prbname;
	struct dt_probe	tmpl;
	struct dt_probe	*probe;

	if (!name)
		return NULL;

	/* Exclude anything that is not a syscalls tracepoint */
	if (strncmp(name, EVENT_PREFIX, sizeof(EVENT_PREFIX) - 1) != 0)
		return NULL;
	name += sizeof(EVENT_PREFIX) - 1;

	if (strncmp(name, ENTRY_PREFIX, sizeof(ENTRY_PREFIX) - 1) == 0) {
		name += sizeof(ENTRY_PREFIX) - 1;
		prbname = "entry";
	} else if (strncmp(name, EXIT_PREFIX, sizeof(EXIT_PREFIX) - 1) == 0) {
		name += sizeof(EXIT_PREFIX) - 1;
		prbname = "return";
	} else
		return NULL;

	memset(&tmpl, 0, sizeof(tmpl));
	tmpl.prv_name = provname;
	tmpl.mod_name = modname;
	tmpl.fun_name = name;
	tmpl.prb_name = prbname;

	probe = dt_probe_by_name(&tmpl);

	return probe;
}

/*
 * Attach the given BPF program (identified by its file descriptor) to the
 * event identified by the given section name.
 */
static int syscall_attach(const char *name, int bpf_fd)
{
	char    efn[256];
	char    buf[256];
	int	event_id, fd, rc;

	name += sizeof(EVENT_PREFIX) - 1;
	strcpy(efn, SYSCALLSFS);
	strcat(efn, name);
	strcat(efn, "/id");

	fd = open(efn, O_RDONLY);
	if (fd < 0) {
		perror(efn);
		return -1;
	}
	rc = read(fd, buf, sizeof(buf));
	if (rc < 0 || rc >= sizeof(buf)) {
		perror(efn);
		close(fd);
		return -1;
	}
	close(fd);
	buf[rc] = '\0';
	event_id = atoi(buf);

	return dt_bpf_attach(event_id, bpf_fd);
}
#endif

dt_provimpl_t	dt_syscall = {
	.name		= "syscall",
	.populate	= &syscall_populate,
	.probe_info	= &syscall_probe_info,
#if 0
	.resolve_event	= &systrace_resolve_event,
	.attach		= &syscall_attach,
#endif
};
