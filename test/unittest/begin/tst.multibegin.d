/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Multiple BEGIN providers
 *
 * SECTION: dtrace Provider
 *
 */


#pragma D option quiet

BEGIN
{
	printf("Begin fired first\n");
}
BEGIN
{
	printf("Begin fired second\n");
}
BEGIN
{
	printf("Begin fired third\n");
}
BEGIN
{
	printf("Begin fired fourth\n");
}
BEGIN
{
	printf("Begin fired fifth\n");
	exit(0);
}
