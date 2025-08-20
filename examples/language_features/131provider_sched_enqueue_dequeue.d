#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./131provider_sched_enqueue_dequeue.d
 *
 *  DESCRIPTION
 *    We can track when processes are enqueued and
 *    dequeued on CPUs.
 */

sched:::dequeue,
sched:::enqueue
{
	printf(" %s %s\n", probename, args[0]->pr_name);
}

/*
 *  A tick probe is used to fire once to stop data collection.
 */
profile:::tick-2sec
{
	exit(0);
}
