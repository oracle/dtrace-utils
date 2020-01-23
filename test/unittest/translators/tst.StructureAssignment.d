/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * If the entire output is copied by means of a structure assignment, any
 * members for which no translation expressions are defined will be filled
 * with zeroes.
 *
 * SECTION: Translators/ Translate Operator
 */

#pragma D option quiet

struct input_struct {
	int ii;
	char ic;
};

struct output_struct {
	int oi;
	char oc;
};


translator struct output_struct < struct input_struct ivar >
{
	oi = ((struct input_struct) ivar).ii;
};

struct output_struct out;
struct input_struct in;

BEGIN
{
	in.ii = 100;
	in.ic = 'z';

	printf("Translating via struct assignment\n");
	out = xlate < struct output_struct > (in);

	printf("out.oi: %d\t out.oc: %d\n", out.oi, out.oc);
}

BEGIN
/(100 != out.oi) || (0 != out.oc)/
{
	exit(1);
}

BEGIN
/(100 == out.oi) && (0 == out.oc)/
{
	exit(0);
}

ERROR
{
	exit(1);
}
