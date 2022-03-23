/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Function calls to alloca()ed space work.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

/*
 * Everything is in an independent clause with its own alloca(),
 * to try to make sure that the verifier's bound proofs don't leak
 * from one test to the next.
 */

BEGIN
{
	x = (char *) alloca(8);
	x[0] = 'a';
	x[1] = '/';
	x[2] = 'b';
	x[3] = 0;
	printf("%s\n", stringof(x));
	trace(x);
}

BEGIN
{
	x = (char *) alloca(8);
	x[0] = 'a';
	x[1] = '/';
	x[2] = 'b';
	x[3] = 0;
	trace(basename(x));
}

BEGIN
{
	x = (char *) alloca(8);
	y = (char *) alloca(8);
	x[0] = 'a';
	x[1] = '/';
	x[2] = 'b';
	x[3] = 0;
	y[0] = '/';
	y[1] = 0;
	trace(index(x, y));
}

BEGIN
{
	x = (char *) alloca(8);
	y = (char *) alloca(8);
	x[0] = 'a';
	x[1] = '/';
	x[2] = 'b';
	x[3] = 0;
	y[0] = '/';
	y[1] = 0;
	trace(rindex(x, y));
}

BEGIN
{
	x = (char *) alloca(8);
	x[0] = 'a';
	x[1] = '/';
	x[2] = 'b';
	x[3] = 0;
	trace(strchr(x, '/'));
}

BEGIN
{
	x = (char *) alloca(8);
	x[0] = 'a';
	x[1] = '/';
	x[2] = 'b';
	x[3] = 0;
	trace(strrchr(x, '/'));
}

BEGIN
{
	x = (char *) alloca(8);
	x[0] = 'a';
	x[1] = '/';
	x[2] = 'b';
	x[3] = 0;
	trace(strlen(x));
}

BEGIN
{
	x = (char *) alloca(8);
	y = (char *) alloca(8);
	x[0] = 'a';
	x[1] = '/';
	x[2] = 'b';
	x[3] = 0;
	y[0] = '/';
	y[1] = 0;
	trace(strjoin(x, y));
}

BEGIN
{
	x = (char *) alloca(8);
	y = (char *) alloca(8);
	x[0] = 'a';
	x[1] = '/';
	x[2] = 'b';
	x[3] = 0;
	y[0] = '/';
	y[1] = 0;
	trace(strstr(x, y));
}

BEGIN
{
	x = (char *) alloca(8);
	y = (char *) alloca(8);
	x[0] = 'a';
	x[1] = '/';
	x[2] = 'b';
	x[3] = 0;
	y[0] = '/';
	y[1] = 0;
	trace(strtok(x, y));
}

BEGIN
{
	x = (char *) alloca(8);
	x[0] = '/';
	x[1] = 0;
	trace(strtok(NULL, x));
}

BEGIN
{
	x = (char *) alloca(8);
	x[0] = 'a';
	x[1] = '/';
	x[2] = 'b';
	x[3] = 0;
	trace(substr(x, 0, 1));
}

BEGIN
{
	exit(0);
}

ERROR
{
	exit(0);
}
