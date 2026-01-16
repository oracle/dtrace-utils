/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Destructive actions may never be speculative.
 *
 * SECTION: Speculative Tracing/Using a Speculation
 */
#pragma D option destructive
#pragma D option quiet

string str;
char a[2];
uintptr_t addr;
size_t maxlen;
BEGIN
{
	self->i = 0;
	addr = (uintptr_t)&a[0];
	maxlen = 10;
	var = speculation();
}

BEGIN
{
	speculate(var);
	printf("Speculation ID: %d", var);
	self->i++;
	copyoutstr(str, addr, maxlen);
}

BEGIN
{
	printf("This test should not have compiled\n");
	exit(0);
}

ERROR
{
	exit(0);
}
