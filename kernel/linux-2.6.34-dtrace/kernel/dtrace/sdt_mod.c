/*
 * FILE:	sdt_mod.c
 * DESCRIPTION:	Statically Defined Tracing: module handling
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/module.h>

#include "dtrace.h"
#include "dtrace_dev.h"
#include "sdt.h"

MODULE_AUTHOR("Kris Van Hees (kris.van.hees@oracle.com)");
MODULE_DESCRIPTION("Statically Defined Tracing");
MODULE_VERSION("v0.1");
MODULE_LICENSE("Proprietary");

static const dtrace_pattr_t sdt_attr = {
};

static dtrace_pops_t sdt_pops = {
};

DT_PROVIDER_MODULE(sdt)
