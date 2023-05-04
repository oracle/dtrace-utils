/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Confirm that runtime-NULL strings can be handled.
 */

#pragma D option quiet

/* set some global variables to run-time NULL */
BEGIN
{
	g1 = strchr("abcdefghij", 'Z');
	g2 = strstr("ABCDEFGHIJ", "a");
	g3 = g1;
}

/* set some (clause-)local variables to run-time NULL */
BEGIN
{
	this->l1 = strchr("abcdefghij", 'Z');
	this->l2 = strstr("ABCDEFGHIJ", "a");
	this->l3 = this->l1;
}

/* set some thread-local variables to run-time NULL */
BEGIN
{
	self->t1 = strchr("abcdefghij", 'Z');
	self->t2 = strstr("ABCDEFGHIJ", "a");
	self->t3 = self->t1;
}

/* set some associative-array variables to run-time NULL */
BEGIN
{
	a1[1234] = strchr("abcdefghij", 'Z');
	a2[1234] = strstr("ABCDEFGHIJ", "a");
	a3[1234] = a1[1234];
}

/* compare some variables */
BEGIN
/g1 != g2 || this->l1 != this->l2 || self->t1 != self->t2 || a1[1234] != a2[1234]/
{
	printf("FAIL A\n");
}

/* compare more variables */
BEGIN
/g1 != g3 || this->l1 != this->l3 || self->t1 != self->t3 || a1[1234] != a3[1234]/
{
	printf("FAIL B\n");
}

/* compare some variables to compile-time NULL */
BEGIN
/g1 != NULL || this->l1 != NULL || self->t1 != NULL || a1[1234] != NULL/
{
	printf("FAIL C\n");
}

/* exit */
BEGIN
{
	printf("done\n");
	exit(0);
}

/* handle any unexpected errors */
ERROR
{
	exit(1);
}
