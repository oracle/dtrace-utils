#!/usr/sbin/dtrace -qs

/*
 *  SYNOPSIS
 *    sudo ./012intro-predicates.d
 *
 *  DESCRIPTION
 *    We have already seen that predicates can be used to
 *    conditionalize execution in a D scripts.  (Other
 *    mechanisms include speculation and ternary operators.)
 */

BEGIN
{
	ntimes = 0;
}

/*
 * The write() system call has a file descriptor in arg0 and a
 * requested number of bytes in arg2.  If a nonzero number of
 * bytes is requested, take note of the fd.
 */
syscall::write:entry
/ (self->nbytes_requested = arg2) != 0 /
{
	ntimes++;
	self->fd = arg0;
}

/*
 * When the write() system call returns, we know the actual
 * number of bytes written.  Report it.
 */
syscall::write:return
/ self->nbytes_requested != 0 /
{
	printf("fd %d: bytes requested %d, actual %d\n",
		self->fd, self->nbytes_requested, arg1);

	/* free memory associated with self-> variables */
	self->nbytes_requested = 0;
	self->fd = 0;
}

/*
 * Only report a limited number of times.
 */
syscall::write:return
/ ntimes >= 5 /
{
	exit(0);
}
