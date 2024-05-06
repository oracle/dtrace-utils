/*
 * Oracle Linux DTrace.
 * Copyright (c) 2012, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * This file defines the standard set of inlines and translators to be made
 * available for all D programs to use to examine process model state.
 */

#pragma D depends_on module vmlinux
#pragma D depends_on library sched.d

typedef time64_t time_t;

typedef struct timestruc {
	time_t tv_sec;
	long tv_nsec;
} timestruc_t;

typedef struct lwpsinfo {
	int pr_flag;			/* lwp flags (DEPRECATED) */
	int pr_lwpid;			/* lwp id */
	uintptr_t pr_addr;		/* internal address of lwp */
	uintptr_t pr_wchan;		/* wait addr for sleeping lwp */
	char pr_stype;			/* sync event type */
	char pr_state;			/* numeric lwp state */
	char pr_sname;			/* printable char for pr_state */
	char pr_nice;			/* nice for cpu usage */
	short pr_syscall;		/* syscall number */
	char pr_oldpri;			/* priority */
	char pr_cpu;			/* CPU usage */
	int pr_pri;			/* priority */
	ushort_t pr_pctcpu;		/* % of recent cpu time */
	ushort_t pr_pad;
	timestruc_t pr_start;		/* lwp start time */
	timestruc_t pr_time;		/* usr+sys cpu time */
	char pr_clname[8];		/* scheduling class name */
	char pr_name[16];		/* name */
	processorid_t pr_onpro;		/* processor last ran on */
	processorid_t pr_bindpro;	/* processor bound to */
	psetid_t pr_bindpset;		/* processor set */
	int pr_lgrp;			/* lwp home lgroup */
	int pr_filler[4];
} lwpsinfo_t;

typedef id_t taskid_t;
typedef id_t dprojid_t;
typedef id_t poolid_t;
typedef id_t zoneid_t;

/*
 * Translate from the kernel's task_struct structure to a Solaris proc(4)
 * psinfo_t struct.
 * We do not provide support for pr_size, pr_rssize, pr_pctcpu, and pr_pctmem.
 * We also do not fill in pr_lwp (the lwpsinfo_t for the representative LWP)
 * because we do not have the ability to select and stop any representative.
 * Also, for the moment, pr_wstat, pr_time, and pr_ctime are not supported,
 * but these could be supported by DTrace in the future using subroutines.
 * Note that any member added to this translator should also be added to the
 * kthread_t-to-psinfo_t translator, below.
 */
typedef struct psinfo {
	int pr_flag;			/* process flags (DEPRECATED) */
	int pr_nlwp;			/* number of active lwps (Linux: 1) */
	pid_t pr_pid;			/* unique process id */
	pid_t pr_ppid;			/* process id of parent */
	pid_t pr_pgid;			/* pid of process group leader */
	pid_t pr_sid;			/* session id */
	uid_t pr_uid;			/* real user id */
	uid_t pr_euid;			/* effective user id */
	uid_t pr_gid;			/* real group id */
	uid_t pr_egid;			/* effective group id */
	uintptr_t pr_addr;		/* address of process */
	size_t pr_size;			/* size of process image (in KB) */
	size_t pr_rssize;		/* resident set sie (in KB) */
	size_t pr_pad1;
	struct tty_struct *pr_ttydev;	/* controlling tty (or -1) */
	ushort_t pr_pctcpu;		/* % of recent cpu time used */
	ushort_t pr_pctmem;		/* % of recent memory used */
	timestruc_t pr_start;		/* process start time */
	timestruc_t pr_time;		/* usr+sys cpu time for process */
	timestruc_t pr_ctime;		/* usr+sys cpu time for children */
	char pr_fname[16];		/* name of exec'd file */
	char pr_psargs[80];		/* initial chars of arg list */
	int pr_wstat;			/* if zombie, wait() status */
	int pr_argc;			/* initial argument count */
	uintptr_t pr_argv;		/* address of initial arg vector */
	uintptr_t pr_envp;		/* address of initial env vector */
	char pr_dmodel;			/* data model */
	char pr_pad2[3];
	taskid_t pr_taskid;		/* task id */
	dprojid_t pr_projid;		/* project id */
	int pr_nzomb;			/* number of zombie lwps (Linux: 0) */
	poolid_t pr_poolid;		/* pool id */
	zoneid_t pr_zoneid;		/* zone id */
	id_t pr_contract;		/* process contract */
	int pr_filler[1];
	lwpsinfo_t pr_lwp;
} psinfo_t;

inline char PR_MODEL_ILP32 = 1;
#pragma D binding "1.0" PR_MODEL_ILP32
inline char PR_MODEL_LP64 = 2;
#pragma D binding "1.0" PR_MODEL_LP64

 
 
inline	int PIDTYPE_PID = 0;
#pragma D binding "1.0" PIDTYPE_PID
inline	int PIDTYPE_TGID = 1;
#pragma D binding "1.0" PIDTYPE_TGID
inline	int PIDTYPE_PGID = 2;
#pragma D binding "1.0" PIDTYPE_PGID
inline	int PIDTYPE_SID = 3;
#pragma D binding "1.0" PIDTYPE_SID

 
 
 
 
 
 
 
#pragma D binding "1.0" translator
translator psinfo_t < struct task_struct *T > {
	pr_nlwp = 1;
	pr_pid = T->tgid;
	pr_ppid = T->real_parent->tgid;
	pr_pgid = T->signal->pids[PIDTYPE_PGID]->numbers[0].nr;
	pr_sid = T->signal->pids[PIDTYPE_SID]->numbers[0].nr;
	pr_uid = T->cred->uid.val;
	pr_euid = T->cred->euid.val;
	pr_gid = T->cred->gid.val;
	pr_egid = T->cred->egid.val;
	pr_addr = (uintptr_t)T;

	pr_ttydev = T->signal->tty ? T->signal->tty
				   : (struct tty_struct *)-1;

	pr_fname = T->comm;
/*
	pr_psargs = stringof(get_psinfo(T)->__psinfo(psargs));
 */
	pr_wstat = 0;
/*
	pr_argc = get_psinfo(T)->__psinfo(argc);
	pr_argv = (uintptr_t)get_psinfo(T)->__psinfo(argv);
	pr_envp = (uintptr_t)get_psinfo(T)->__psinfo(envp);
 */

	pr_dmodel = PR_MODEL_LP64;

	pr_taskid = 0;
	pr_projid = 0;
	pr_nzomb = 0;
	pr_poolid = 0;
	pr_zoneid = 0;
	pr_contract = 0;
};

inline int PR_STOPPED = 0x00000001;
#pragma D binding "1.0" PR_STOPPED
inline int PR_ISTOP = 0x00000002;
#pragma D binding "1.0" PR_ISTOP
inline int PR_DSTOP = 0x00000004;
#pragma D binding "1.0" PR_DSTOP
inline int PR_IDLE = 0x10000000;
#pragma D binding "1.0" PR_IDLE

inline char SSLEEP = 1;
#pragma D binding "1.0" SSLEEP
inline char SRUN = 2;
#pragma D binding "1.0" SRUN
inline char SZOMB = 3;
#pragma D binding "1.0" SZOMB
inline char SSTOP = 4;
#pragma D binding "1.0" SSTOP
inline char SIDL = 5;
#pragma D binding "1.0" SIDL
inline char SONPROC = 6;
#pragma D binding "1.0" SONPROC
inline char SWAIT = 7;
#pragma D binding "1.0" SWAIT

/*
 * Translate from the kernel's task_struct structure to a Solaris proc(4)
 * lwpsinfo_t.
 * We do not provide support for pr_nice, pr_oldpri, pr_cpu, or pr_pctcpu.
 * Also, for the moment, pr_start and pr_time are not supported, but these
 * could be supported by DTrace in the future using subroutines.
 */
#pragma D binding "1.0" translator
translator lwpsinfo_t < struct task_struct *T > {
	pr_flag = (T->__state & 0x00000004) ? PR_STOPPED : 0;
/*
	pr_flag = ((T->t_state == TS_STOPPED) ? (PR_STOPPED |
	    ((!(T->t_schedflag & TS_PSTART)) ? PR_ISTOP : 0)) :
	    ((T->t_proc_flag & TP_PRVSTOP) ? PR_STOPPED | PR_ISTOP : 0)) |
	    ((T == T->t_procp->p_agenttp) ? PR_AGENT : 0) |
	    ((!(T->t_proc_flag & TP_TWAIT)) ? PR_DETACH : 0) |
	    ((T->t_proc_flag & TP_DAEMON) ? PR_DAEMON : 0) |
	    ((T->t_procp->p_pidflag & CLDNOSIGCHLD) ? PR_NOSIGCHLD : 0) |
	    ((T->t_procp->p_pidflag & CLDWAITPID) ? PR_WAITPID : 0) |
	    ((T->t_procp->p_proc_flag & P_PR_FORK) ? PR_FORK : 0) |
	    ((T->t_procp->p_proc_flag & P_PR_RUNLCL) ? PR_RLC : 0) |
	    ((T->t_procp->p_proc_flag & P_PR_KILLCL) ? PR_KLC : 0) |
	    ((T->t_procp->p_proc_flag & P_PR_ASYNC) ? PR_ASYNC : 0) |
	    ((T->t_procp->p_proc_flag & P_PR_BPTADJ) ? PR_BPTADJ : 0) |
	    ((T->t_procp->p_proc_flag & P_PR_PTRACE) ? PR_PTRACE : 0) |
	    ((T->t_procp->p_flag & SMSACCT) ? PR_MSACCT : 0) |
	    ((T->t_procp->p_flag & SMSFORK) ? PR_MSFORK : 0) |
	    ((T->t_procp->p_flag & SVFWAIT) ? PR_VFORKP : 0) |
	    (((T->t_procp->p_flag & SSYS) ||
	    (T->t_procp->p_as == &`kas)) ? PR_ISSYS : 0) |
	    ((T == T->t_cpu->cpu_idle_thread) ? PR_IDLE : 0);
*/

	pr_lwpid = T->pid;
	pr_addr = (uintptr_t)T;
	pr_wchan = NULL;
	pr_stype = 0;

	pr_state = (T->__state & 0x00000001) ? SSLEEP :
		   (T->__state & 0x00000002) ? SSLEEP :
		   (T->__state & 0x00000004) ? SSTOP :
		   (T->__state & 0x00000008) ? SSTOP :
		   (T->__state & 0x00000020) ? SZOMB :
		   (T->__state & 0x00000010) ? SZOMB :
		   (T->__state & 0x00000080) ? SZOMB :
		   (T->__state & 0x00000100) ? SWAIT :
		   (T->__state & 0x00000200) ? SWAIT : SRUN;
	pr_sname = (T->__state & 0x00000001) ? 'S' :
		   (T->__state & 0x00000002) ? 'S' :
		   (T->__state & 0x00000004) ? 'T' :
		   (T->__state & 0x00000008) ? 'T' :
		   (T->__state & 0x00000020) ? 'Z' :
		   (T->__state & 0x00000010) ? 'Z' :
		   (T->__state & 0x00000080) ? 'Z' :
		   (T->__state & 0x00000100) ? 'W' :
		   (T->__state & 0x00000200) ? 'W' : 'R';

	pr_pri = T->prio;
	pr_name = T->comm;
	pr_onpro = ((struct thread_info *)T->stack)->cpu;
};

inline psinfo_t *curpsinfo = xlate <psinfo_t *> (curthread);
#pragma D attributes Stable/Stable/Common curpsinfo
#pragma D binding "1.0" curpsinfo

inline lwpsinfo_t *curlwpsinfo = xlate <lwpsinfo_t *> (curthread);
#pragma D attributes Stable/Stable/Common curlwpsinfo
#pragma D binding "1.0" curlwpsinfo

inline string cwd = d_path(&(curthread->fs->pwd));
#pragma D attributes Stable/Stable/Common cwd
#pragma D binding "1.0" cwd

inline string root = d_path(&(curthread->fs->root));
#pragma D attributes Stable/Stable/Common root
#pragma D binding "1.0" root

inline int CLD_EXITED = 1;
#pragma D binding "1.0" CLD_EXITED
inline int CLD_KILLED = 2;
#pragma D binding "1.0" CLD_KILLED
inline int CLD_DUMPED = 3;
#pragma D binding "1.0" CLD_DUMPED
inline int CLD_TRAPPED = 4;
#pragma D binding "1.0" CLD_TRAPPED
inline int CLD_STOPPED = 5;
#pragma D binding "1.0" CLD_STOPPED
inline int CLD_CONTINUED = 6;
#pragma D binding "1.0" CLD_CONTINUED

/*
 * Below we provide some inline definitions to simplify converting from
 * a struct pid * to a pid_t.  structpid2pid is a DTrace-ized version of
 * pid_vnr().
 */
 /*
inline	struct pid_namespace *pid_namespace[struct pid *p] = p != NULL ?
	p->numbers[p->level].ns : NULL;

inline	int pid_namespace_level[struct pid *p] = p != NULL && pid_namespace[p] ?
	pid_namespace[p]->level : 0;

m4_dnl 4.19 and above use thread_pid for this.
define_for_kernel([[__task_pid]], [[ m4_dnl
	(m4_kver(4, 19, 0), [[task->thread_pid]])]], m4_dnl
	[[task->pids[PIDTYPE_PID].pid]]) m4_dnl

inline	struct pid *task_pid[struct task_struct *task] =
	task != NULL ? __task_pid : NULL;

inline	int current_namespace_level = pid_namespace_level[task_pid[curthread]];

inline	pid_t structpid2pid[struct pid *p] =
	p != NULL && current_namespace_level <= p->level &&
	((struct upid *)&(p->numbers[current_namespace_level]))->ns ==
	pid_namespace[task_pid[curthread]] ?
	((struct upid *)&(p->numbers[current_namespace_level]))->nr : 0;
*/
