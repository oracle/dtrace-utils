/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@skip: Solaris-specific, unconverted */

/*
 * When our strings(1) invocation starts a read(2), set a watched flag on
 * the current thread.  When the read(2) finishes, clear the watched flag.
 */
syscall::read:entry
/curpsinfo->pr_psargs == "strings -a /dev/ksyms"/
{
	printf("read %u bytes to user address %x\n", arg2, arg1);
	self->watched = 1;
}

syscall::read:return
/self->watched/
{
	self->watched = 0;
}

/*
 * Instrument uiomove(9F).  The prototype for this function is as follows:
 * int uiomove(caddr_t addr, size_t nbytes, enum uio_rw rwflag, uio_t *uio);
 */
fbt::uiomove:entry
/self->watched/
{
	this->iov = args[3]->uio_iov;

	printf("uiomove %u bytes to %p in pid %d\n",
	    this->iov->iov_len, this->iov->iov_base, pid);
}
