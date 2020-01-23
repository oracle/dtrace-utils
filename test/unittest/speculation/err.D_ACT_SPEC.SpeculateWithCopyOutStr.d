#!/usr/sbin/dtrace -ws
/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * Destructive actions may never be speculative.
 *
 * SECTION: Speculative Tracing/Using a Speculation
 * SECTION: dtrace(1M) Utility/ -w option
 */
#pragma D option quiet

string str;
char a[2];
uintptr_t addr;
size_t maxlen;
BEGIN
{
	self->i = 0;
	addr = (uintptr_t) &a[0];
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
