/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: depends on a Solaris-specific DIF subroutine */

#pragma D option quiet

BEGIN
{
	this->dev = (struct dev_info *)alloca(sizeof (struct dev_info));
	this->minor1 =
	    (struct ddi_minor_data *)alloca(sizeof (struct ddi_minor_data));
	this->minor2 =
	    (struct ddi_minor_data *)alloca(sizeof (struct ddi_minor_data));
	this->minor3 =
	    (struct ddi_minor_data *)alloca(sizeof (struct ddi_minor_data));

	this->minor1->d_minor.dev = 0;
	this->minor1->next = this->minor2;

	this->minor2->d_minor.dev = 0;
	this->minor2->next = this->minor3;

	this->minor3->d_minor.dev = 0;
	this->minor3->next = this->minor1;

	this->dev->devi_minor = this->minor1;
	trace(ddi_pathname(this->dev, 1));
}

ERROR
/arg4 == DTRACEFLT_ILLOP/
{
	exit(0);
}

ERROR
/arg4 != DTRACEFLT_ILLOP/
{
	exit(1);
}
