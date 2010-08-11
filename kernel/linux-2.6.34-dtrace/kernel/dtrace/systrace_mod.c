/*
 * FILE:	systrace_mod.c
 * DESCRIPTION:	System Call Tracing: module handling
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/module.h>

#include "dtrace.h"
#include "dtrace_dev.h"
#include "systrace.h"

MODULE_AUTHOR("Kris Van Hees (kris.van.hees@oracle.com)");
MODULE_DESCRIPTION("System Call Tracing");
MODULE_VERSION("v0.1");
MODULE_LICENSE("Proprietary");

static const dtrace_pattr_t systrace_attr = {
};

static dtrace_pops_t systrace_pops = {
};

DT_PROVIDER_MODULE(systrace);
