/*
 * FILE:	profile_mod.c
 * DESCRIPTION:	Profile Interrupt Tracing: module handling
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/module.h>

#include "dtrace.h"
#include "dtrace_dev.h"
#include "profile.h"

MODULE_AUTHOR("Kris Van Hees (kris.van.hees@oracle.com)");
MODULE_DESCRIPTION("Profile Interrupt Tracing");
MODULE_VERSION("v0.1");
MODULE_LICENSE("Proprietary");

static const dtrace_pattr_t profile_attr = {
};

static dtrace_pops_t profile_pops = {
};

DT_PROVIDER_MODULE(profile)
