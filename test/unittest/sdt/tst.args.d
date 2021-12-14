/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: -C */

#pragma D option destructive
#pragma D option quiet

/*
 * Ensure that arguments to SDT probes can be retrieved correctly.
 */
BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
	system("ls >/dev/null");
}

/*
 * The 'task_rename' tracepoint passes the following arguments:
 *
 *	pid_t	pid
 *	char	oldcomm[16]
 *	char	newcomm[16]
 *	short	omm_score_adj
 *
 * Arguments are passed as 64-bit values in arg0 through arg9, so they need
 * to be decoded into their respective values to correspond to the typed data
 * shown above.
 *
 * For LSB host byte order (x86_64) we have:
 *
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * arg0 | o[3]  | o[2]  | o[1]  | o[0]  |  pid  |  pid  |  pid  |  pid  |
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * arg1 | o[11] | o[10] | o[9]  | o[8]  | o[7]  | o[6]  | o[5]  | o[4]  |
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * arg2 | n[3]  | n[2]  | n[1]  | n[0]  | o[15] | o[14] | o[13] | o[12] |
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * arg3 | n[11] | n[10] | n[9]  | n[8]  | n[7]  | n[6]  | n[5]  | n[4]  |
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * arg4 |   -   |   -   | adj   | adj   | n[15] | n[14] | n[13] | n[12] |
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 */
this char v;
this bool done;

sdt:task::task_rename
/(int)arg0 == pid/
{
#define putchar(c)	this->v = (c); \
			this->v = this->done ? 0 : this->v; \
			this->done = !this->v; \
			printf("%c",  this->v);

	printf("PID OK, oldcomm ");
	this->done = 0;
	putchar(arg0 >> 32);
	putchar(arg0 >> 40);
	putchar(arg0 >> 48);
	putchar(arg0 >> 56);
	putchar(arg1);
	putchar(arg1 >> 8);
	putchar(arg1 >> 16);
	putchar(arg1 >> 24);
	putchar(arg1 >> 32);
	putchar(arg1 >> 40);
	putchar(arg1 >> 48);
	putchar(arg1 >> 56);
	putchar(arg2);
	putchar(arg2 >> 8);
	putchar(arg2 >> 16);
	putchar(arg2 >> 24);
	printf(", newcomm ");
	this->done = 0;
	putchar(arg2 >> 32);
	putchar(arg2 >> 40);
	putchar(arg2 >> 48);
	putchar(arg2 >> 56);
	putchar(arg3);
	putchar(arg3 >> 8);
	putchar(arg3 >> 16);
	putchar(arg3 >> 24);
	putchar(arg3 >> 32);
	putchar(arg3 >> 40);
	putchar(arg3 >> 48);
	putchar(arg3 >> 56);
	putchar(arg4);
	putchar(arg4 >> 8);
	putchar(arg4 >> 16);
	putchar(arg4 >> 24);
	printf(", oom_score_adj %hd\n", (short)arg4 >> 48);
	exit(0);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
