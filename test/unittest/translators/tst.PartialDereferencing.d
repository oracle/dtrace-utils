/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * If you dereference a single member of the translation input, then the
 * compiler will generate the code corresponding to that member
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
	oc = ((struct input_struct) ivar).ic;
};

struct output_struct out;
struct input_struct in;

BEGIN
{
	in.ii = 100;
	in.ic = 'z';

	printf("Translating only a part of the input struct\n");
	out.oi = xlate < struct output_struct > (in).oi;

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
