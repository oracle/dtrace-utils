#!/usr/sbin/dtrace -qs

/*
 *  SYNOPSIS
 *    sudo ./311actions-rand.d
 *
 *  DESCRIPTION
 *    The rand() function generates a random integer.
 */

BEGIN
{
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);

	exit(0);
}
