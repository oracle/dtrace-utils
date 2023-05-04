/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	self->t1 = strchr("abcdefghij", 'Z');
	self->t2 = strstr("ABCDEFGHIJ", "a");
	self->t3 = self->t1;
}

BEGIN
/self->t1 != self->t2 || self->t1 != self->t2 || self->t1 != self->t3 || self->t1 != NULL/
{
	printf("FAIL\n");
}

BEGIN
{
	printf("done\n");
	exit(0);
}

ERROR
{
	exit(1);
}
