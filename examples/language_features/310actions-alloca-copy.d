#!/usr/sbin/dtrace -qs

#pragma D option strsize=32

/*
 *  SYNOPSIS
 *    sudo ./310actions-alloca-copy.d
 *
 *  DESCRIPTION
 *    A DTrace program has scratch memory that it manages
 *    for its own purposes.  The program can allocate scratch
 *    memory as well as copy into, out of, and between buffers.
 *    It is automatically freed at the end of each clause.
 */

/* alloca */
BEGIN
{
	/* alloca() a string */
	ptr = (string *) alloca(32);

	/* set its contents (truncated to strsize chars, including NUL) */
	*ptr = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	/* print it */
	trace(*ptr);

	/* end of the clause:  ptr is freed */
}

/* bcopy */
BEGIN
{
	/* alloca() a string */
	ptr = (string *) alloca(32);

	/* copy from kernel memory to scratch memory */
	bcopy(`linux_banner, ptr, 32);

	/* print it */
	printf("\n%s", *ptr);

	/* end of the clause:  ptr is freed */
}

/* copyinto */
syscall::write:entry
{
	/* alloca() a string */
	ptr = (string *) alloca(32);

	/* copy from user space into scratch memory */
	copyinto(arg1, 32, ptr);

	/* print it */
	printf("\n%s to fd %d, %d bytes", *ptr, arg0, arg2);

	/* end of the clause:  ptr is freed */
}

/* copyin */
syscall::write:entry
{
	/* copy from user space to scratch memory, allocating the pointer */
	ptr = copyin(arg1, 32);

	/* print it */
	printf("\n%s to fd %d, %d bytes", *ptr, arg0, arg2);

	/* end of the clause:  ptr is freed */
}

/* copyinstr */
syscall::write:entry
{
	/* copy string from user space to scratch memory, allocating the pointer */
	str = copyinstr(arg1);

	/* print it */
	printf("\n%s to fd %d, %d bytes", str, arg0, arg2);

	/* end of the clause:  ptr is freed */
}

/*
 * There are also copyout() and copyoutstr() functions, that may be
 * used to copy from scratch memory to user-space buffers.  Since they
 * alter the behavior of the user code, the "destructive" option -w
 * must be specified.
 */

syscall::write:entry
{
	exit(0);
}
