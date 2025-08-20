#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./203built-in-variables-process-info.d
 *
 *  DESCRIPTION
 *    A number of built-in variables describe the process
 *    that is running.
 */

syscall::write:entry
{
	printf("\n\n");
	printf("The CPU is %d (%d).\n", curcpu->cpu_id, cpu);

	printf("\n");
	printf("The exec name can be accessed through\n");
	printf("    the struct task_struct: %s\n", curthread->comm);
	printf("    or a built-in variable: %s\n", execname);

	printf("\n");
	printf("The execargs are: %s\n", execargs);

	printf("\n");
	printf("The group id and user id are %d and %d.\n", gid, uid);
	printf("The process id and thread id are %d and %d.\n", pid, tid);
	printf("The parent process id is %d.\n\n", ppid);

	exit(0);
}
