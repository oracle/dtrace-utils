/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@trigger: readwholedir */

struct callinfo {
	uint64_t ts;      /* timestamp of last syscall entry */
	uint64_t elapsed; /* total elapsed time in nanoseconds */
	uint64_t calls;   /* number of calls made */
	size_t maxbytes;  /* maximum byte count argument */
};

struct callinfo i[string];	/* declare i as an associative array */

syscall::read:entry, syscall::write:entry
/pid == $target/
{
	i[probefunc].ts = timestamp;
	i[probefunc].calls++;
	i[probefunc].maxbytes = arg2 > i[probefunc].maxbytes ?
		arg2 : i[probefunc].maxbytes;
}

syscall::read:return, syscall::write:return
/i[probefunc].ts != 0 && pid == $target/
{
	i[probefunc].elapsed += timestamp - i[probefunc].ts;
}

syscall::exit_group:entry
/pid == $target/
{
       exit(0);
}

END
{
	printf("\n        calls  max bytes  elapsed\n");
	printf("------  -----  ---------  -------\n");
	printf("  read  %5d  %9d  %s\n",
	    i["read"].calls, i["read"].maxbytes, i["read"].elapsed != 0 ? "yes" : "no");
	printf(" write  %5d  %9d  %s\n",
	    i["write"].calls, i["write"].maxbytes, i["write"].elapsed != 0 ? "yes" : "no");
}
