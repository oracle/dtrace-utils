/*
 * FILE:	dtrace_ptofapi.c
 * DESCRIPTION:	Dynamic Tracing: provider-to-framework API
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/module.h>

#include "dtrace.h"

int dtrace_register(const char *name, const dtrace_pattr_t *pap, uint32_t priv,
		    cred_t *cr, const dtrace_pops_t *pops, void *arg,
		    dtrace_provider_id_t *idp)
{
	if (name == NULL || pap == NULL || pops == NULL || idp == NULL)
		return -EINVAL;

	if (name[0] == '\0')
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(dtrace_register);

int dtrace_unregister(dtrace_provider_id_t id)
{
	return 0;
}
EXPORT_SYMBOL(dtrace_unregister);
