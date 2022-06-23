#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# This test for the subroutines
#     rw_read_held()
#     rw_write_held()
#     rw_iswriter()
# may prove not to be very robust.  It does not identify a workload that
# is known to exercise the read-write locks well.  So we simply sit on
# _raw_read_lock and _raw_write_lock FBT probes and hope there is some
# activity;  test runs may time out.  The .r file assumes the locks are
# uncontested.

# @@tags: unstable

dtrace=$1
tmpfile=$tmpdir/tst.rw_.$$

$dtrace $dt_flags -qn '

/* desired numbers of read and write events */

BEGIN
{
	nr = 4;
	nw = 1;
}

/* read events */

fbt:vmlinux:_raw_read_lock:entry
/nr > 0/
{
	self->rlock = (rwlock_t *)arg0;
	nr--;
	printf("read  entry  %x %x %x\n",
		0 != rw_iswriter(self->rlock),
		0 != rw_read_held(self->rlock),
		0 != rw_write_held(self->rlock));
}

fbt:vmlinux:_raw_read_lock:return
/self->rlock/
{
	printf("read  return %x %x %x\n",
		0 != rw_iswriter(self->rlock),
		0 != rw_read_held(self->rlock),
		0 != rw_write_held(self->rlock));
	self->rlock = 0;
}

/* write events */

fbt:vmlinux:_raw_write_lock:entry
/nw > 0/
{
	self->wlock = (rwlock_t *)arg0;
	nw--;
	printf("write entry  %x %x %x\n",
		0 != rw_iswriter(self->wlock),
		0 != rw_read_held(self->wlock),
		0 != rw_write_held(self->wlock));
}

fbt:vmlinux:_raw_write_lock:return
/self->wlock/
{
	printf("write return %x %x %x\n",
		0 != rw_iswriter(self->wlock),
		0 != rw_read_held(self->wlock),
		0 != rw_write_held(self->wlock));
	self->wlock = 0;
}

/* termination */

fbt:vmlinux:_raw_read_lock:return,
fbt:vmlinux:_raw_write_lock:return
/nr == 0 && nw == 0/
{
	printf("finish\n");
	exit(0);
}
' >& $tmpfile

if [ $? -ne 0 ]; then
	echo "DTrace error"
	cat $tmpfile
	exit 1
fi

sort $tmpfile

exit 0

