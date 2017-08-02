/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: -q $_pid */
/* @@xfail: userspace tracing not implemented */
/* @@skip: solaris-specific, not yet converted */

pid$1:libkstat:kstat_data_lookup:entry
{
	self->ksname = arg1;
}

pid$1:libkstat:kstat_data_lookup:return
/self->ksname != NULL && arg1 != NULL/
{
	this->ksp = (kstat_named_t *) copyin(arg1, sizeof (kstat_named_t));
	printf("%s has ui64 value %u\n",
	    copyinstr(self->ksname), this->ksp->value.ui64);
}

pid$1:libkstat:kstat_data_lookup:return
/self->ksname != NULL && arg1 == NULL/
{
	self->ksname = NULL;
}
