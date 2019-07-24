/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The core probe provider for DTrace for the BEGIN, END, and ERROR probes.
 */
#include <string.h>

#if 0
#include "dtrace_impl.h"
#endif
#include "dt_provider.h"

static const char	provname[] = "dtrace";

static int dtrace_populate(void)
{
#if 0
	dt_probe_new(&dt_dtrace, provname, NULL, NULL, "BEGIN", 0, 0);
	dt_probe_new(&dt_dtrace, provname, NULL, NULL, "END", 0, 0);
	dt_probe_new(&dt_dtrace, provname, NULL, NULL, "ERROR", 0, 0);
#endif

	return 3;
}

#if 0
#define EVENT_PREFIX	"tracepoint/dtrace/"

/*
 * Perform a probe lookup based on an event name (usually obtained from a BPF
 * ELF section name).  We use an unused event group (dtrace) to be able to
 * fake a section name that libbpf will allow us to use.
 */
static struct dt_probe *dtrace_resolve_event(const char *name)
{
	struct dt_probe	tmpl;
	struct dt_probe	*probe;

	if (!name)
		return NULL;

	/* Exclude anything that is not a dtrace core tracepoint */
	if (strncmp(name, EVENT_PREFIX, sizeof(EVENT_PREFIX) - 1) != 0)
		return NULL;
	name += sizeof(EVENT_PREFIX) - 1;

	memset(&tmpl, 0, sizeof(tmpl));
	tmpl.prv_name = provname;
	tmpl.mod_name = NULL;
	tmpl.fun_name = NULL;
	tmpl.prb_name = name;

	probe = dt_probe_by_name(&tmpl);

	return probe;
}

static int dtrace_attach(const char *name, int bpf_fd)
{
	return -1;
}
#endif

dt_provmod_t	dt_dtrace = {
	.name		= "dtrace",
	.populate	= &dtrace_populate,
#if 0
	.resolve_event	= &dtrace_resolve_event,
	.attach		= &dtrace_attach,
#endif
};
