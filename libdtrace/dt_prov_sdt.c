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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/bpf.h>
#include <sys/stat.h>
#include <sys/types.h>

#if 0
#include "dtrace_impl.h"
#endif
#include "dt_provider.h"

static const char	provname[] = "sdt";
static const char	modname[] = "vmlinux";

#define PROBE_LIST	TRACEFS "available_events"

#define KPROBES		"kprobes:"
#define SYSCALLS	"syscalls:"

/*
 * The PROBE_LIST file lists all tracepoints in a <group>:<name> format.  When
 * kprobes are registered on the system, they will appear in this list also as
 * kprobes:<name>.  We need to ignore them because DTrace already accounts for
 * them as FBT probes.
 */
static int sdt_populate(void)
{
	FILE			*f;
	char			buf[256];
	char			*p;
	int			n = 0;

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return 0;

	while (fgets(buf, sizeof(buf), f)) {
		p = strchr(buf, '\n');
		if (p)
			*p = '\0';

		p = strchr(buf, ':');
		if (p == NULL) {
#if 0
			dt_probe_new(&dt_sdt, provname, modname, NULL, buf,
				     0, 0);
#endif
			n++;
		} else if (memcmp(buf, KPROBES, sizeof(KPROBES) - 1) == 0) {
			continue;
		} else if (memcmp(buf, SYSCALLS, sizeof(SYSCALLS) - 1) == 0) {
			continue;
		} else {
			*p++ = '\0';

#if 0
			dt_probe_new(&dt_sdt, provname, buf, NULL, p, 0, 0);
#endif
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

dt_provmod_t	dt_sdt = {
	.name		= "sdt",
	.populate	= &sdt_populate,
#if 0
	.resolve_event	= &sdt_resolve_event,
	.attach		= &sdt_attach,
#endif
};
