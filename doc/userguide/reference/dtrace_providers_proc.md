
# Proc Provider {#dt_ref_proc_prov}

The `proc` provider makes available the probes that pertain to the following activities: process creation and termination, LWP creation and termination, execution of new program images, and signal sending and handling.



**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## proc Probes {#dt_ref_procprobes_prov}

The probes for the `proc` provider are listed in the following table.

<table><thead><tr><th>

Probe

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`create`

</td><td>

Fires when a process \(or process thread\) is created using `fork()` or `vfork()`, which both invoke `clone()`. The `psinfo_t` corresponding to the new child process is pointed to by `args[0]`.

</td></tr><tr><td>

`exec`

</td><td>

Fires whenever a process loads a new process image using a variant of the `execve()` system call. The `exec` probe fires before the process image is loaded. Process variables like `execname` and `curpsinfo` therefore contain the process state before the image is loaded. Some time after the `exec` probe fires, either the `exec-failure` or `exec-success` probe subsequently fires in the same thread. The path of the new process image is pointed to by `args[0]`.

</td></tr><tr><td>

`exec-failure`

</td><td>

Fires when an `exec()` variant has failed. The `exec-failure` probe fires only after the `exec` probe has fired in the same thread. The `errno` value is provided in `args[0]`.

</td></tr><tr><td>

`exec-success`

</td><td>

Fires when an `exec()` variant has succeeded. Like the `exec-failure` probe, the `exec-success` probe fires only after the `exec` probe has fired in the same thread. By the time that the `exec-success` probe fires, process variables like `execname` and `curpsinfo` contain the process state after the new process image has been loaded.

</td></tr><tr><td>

`exit`

</td><td>

Fires when the current process is exiting. The reason for exit, which is expressed as one of the `SIGCHLD` `<asm-generic/signal.h>` codes, is contained in `args[0]`.

</td></tr><tr><td>

`lwp-create`

</td><td>

Fires when a process thread is created, the latter typically as a result of `pthread_create()`. The `lwpsinfo_t` corresponding to the new thread is pointed to by `args[0]`. The `psinfo_t` of the process that created the thread is pointed to by `args[1]`.

</td></tr><tr><td>

`lwp-exit`

</td><td>

Fires when a process or process thread is exiting, due either to a signal or to an explicit call to `exit` or `pthread_exit()`.

</td></tr><tr><td>

`lwp-start`

</td><td>

Fires within the context of a newly created process or process thread. The `lwp-start` probe fires before any user-level instructions are executed. If the thread is the first created for the process, the `start` probe fires, followed by `lwp-start`.

</td></tr><tr><td>

`signal-clear`

</td><td>

Probes that fires when a pending signal is cleared because the target thread was waiting for the signal in `sigwait()`, `sigwaitinfo()`, or `sigtimedwait()`. Under these conditions, the pending signal is cleared and the signal number is returned to the caller. The signal number is in `args[0]`. `signal-clear` fires in the context of the formerly waiting thread.

</td></tr><tr><td>

`signal-discard`

</td><td>

Fires when a signal is sent to a single-threaded process and the signal is both unblocked and ignored by the process. Under these conditions, the signal is discarded on generation. The `lwpsinfo_t` and `psinfo_t` of the target process and thread are in `args[0]` and `args[1]`, respectively. The signal number is in `args[2]`.

</td></tr><tr><td>

`signal-handle`

</td><td>

Fires immediately before a thread handles a signal. The `signal-handle` probe fires in the context of the thread that will handle the signal. The signal number is in `args[0]`. A pointer to the `siginfo_t` structure that corresponds to the signal is in `args[1]`. The address of the signal handler in the process is in `args[2]`.

</td></tr><tr><td>

`signal-send`

</td><td>

Fires when a signal is sent to a process or to a thread created by a process. The `signal-send` probe fires in the context of the sending process or thread. The `lwpsinfo_t` and `psinfo_t` of the receiving process and thread are in `args[0]` and `args[1]`, respectively. The signal number is in `args[2]`. signal-send is always followed by `signal-handle` or `signal-clear` in the receiving process and thread.

</td></tr><tr><td>

`start`

</td><td>

Fires in the context of a newly created process. The `start` probe fires before any user-level instructions are executed in the process.

</td></tr><tbody></table>
**Note:**

No fundamental difference between a process and a thread that a process creates, exists in Linux. The threads of a process are set up so that they can share resources, but each thread has its own entry in the process table with its own process ID.

## proc Probe Arguments {#dt_ref_procargs_prov}

The following table lists the argument types for the `proc` probes. See [proc Probes](dtrace_providers_proc.md#) for a description of the arguments. The `argN` are implementation specific. Use `args[]` to access the probe arguments.

<table><thead><tr><th>

Probe

</th><th>

`args[0]`

</th><th>

`args[1]`

</th><th>

`args[2]`

</th></tr></thead><tbody><tr><td>

`create`

</td><td>

`psinfo_t *`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`exec`

</td><td>

`char *`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`exec-failure`

</td><td>

`int`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`exec-success`

</td><td>

—

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`exit`

</td><td>

`int`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`lwp-create`

</td><td>

`lwpsinfo_t *`

</td><td>

`psinfo_t *`

</td><td>

—

</td></tr><tr><td>

`lwp-exit`

</td><td>

—

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`lwp-start`

</td><td>

—

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`signal-clear`

</td><td>

`int`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`signal-discard`

</td><td>

`lwpsinfo_t *`

</td><td>

`psinfo_t *`

</td><td>

`int`

</td></tr><tr><td>

`signal-handle`

</td><td>

`int`

</td><td>

`siginfo_t *`

</td><td>

`void (*)(void)`

</td></tr><tr><td>

`signal-send`

</td><td>

`lwpsinfo_t *`

</td><td>

`psinfo_t *`

</td><td>

`int`

</td></tr><tr><td>

`start`

</td><td>

—

</td><td>

—

</td><td>

—

</td></tr><tbody></table>
### lwpsinfo\_t {#dt_ref_lwpsinfoproc_prov}

Several `proc` probes have arguments of type `lwpsinfo_t`. Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/procfs.d`. Some structure members, while still recognized for historical reasons, aren't implemented. The definition of the `lwpsinfo_t` structure is as follows:

```nocopybutton
typedef struct lwpsinfo {       
        int pr_flag;                    /* lwp flags (DEPRECATED) */  
        int pr_lwpid;                   /* lwp id */
        uintptr_t pr_addr;              /* internal address of lwp */
        uintptr_t pr_wchan;             /* NOT IMPLEMENTED */
        char pr_stype;                  /* NOT IMPLEMENTED */
        char pr_state;                  /* numeric lwp state */
        char pr_sname;                  /* printable char for pr_state */
        char pr_nice;                   /* NOT IMPLEMENTED */
        short pr_syscall;               /* NOT IMPLEMENTED */
        char pr_oldpri;                 /* NOT IMPLEMENTED */
        char pr_cpu;                    /* NOT IMPLEMENTED */
        int pr_pri;                     /* priority */
        ushort_t pr_pctcpu;             /* NOT IMPLEMENTED */
        ushort_t pr_pad;                /* struct padding */
        timestruc_t pr_start;           /* NOT IMPLEMENTED */
        timestruc_t pr_time;            /* NOT IMPLEMENTED */
        char pr_clname[8];              /* NOT IMPLEMENTED */
        char pr_name[16];               /* name */
        processorid_t pr_onpro;         /* processor last ran on */
        processorid_t pr_bindpro;       /* NOT IMPLEMENTED */
        psetid_t pr_bindpset;           /* NOT IMPLEMENTED */
        int pr_lgrp;                    /* NOT IMPLEMENTED */
        int pr_filler[4];               /* struct padding */

} lwpsinfo_t;
```

**Note:**

Lightweight processes don't exist in Linux. Rather, in Linux, processes and threads are represented by process descriptors of type `struct task_struct` in the task list. DTrace translates the members of `lwpsinfo_t` from the `task_struct` for the Linux process.

The `pr_flag` is set to `1` if the thread is stopped. Otherwise, it's set to `0`.

In Linux, the `pr_stype` field is unsupported, and hence is always `0`.

The following table describes the values that `pr_state` can take, including the corresponding character values for `pr_sname`.

<table><thead><tr><th>

`pr_state` Value
</th><th>

`pr_sname` Value
</th><th>

Description
</th></tr></thead><tbody><tr><td>

`SRUN` \(2\)

</td><td>

`R`
</td><td>

The thread is runnable or is running on a CPU. The `sched:::enqueue` probe fires immediately before a thread's state is transitioned to `SRUN`. The `sched:::on-cpu` probe will fire a short time after the thread starts to run.

 The equivalent Linux task state is `TASK_RUNNING`.

</td></tr><tr><td>

`SSLEEP` \(1\)

</td><td>

`S`
</td><td>

The thread is sleeping. The `sched:::sleep` probe will fire immediately before a thread's state is transitioned to `SSLEEP`.

 The equivalent Linux task state is `TASK_INTERRUPTABLE` or `TASK_UNINTERRUPTABLE`.

</td></tr><tr><td>

`SSTOP` \(4\)

</td><td>

`T`
</td><td>

The thread is stopped, either because of an explicit `proc` directive or some other stopping mechanism.

 The equivalent Linux task state is `__TASK_STOPPED` or `__TASK_TRACED`.

</td></tr><tr><td>

`SWAIT` \(7\)

</td><td>

`W`
</td><td>

The thread is waiting on wait queue. The `sched:::cpucaps-sleep` probe will fire immediately before the thread's state transitions to `SWAIT`.

 The equivalent Linux task state is `TASK_WAKEKILL` or `TASK_WAKING`.

</td></tr><tr><td>

`SZOMB` \(3\)

</td><td>

`Z`
</td><td>

The thread is a zombie.

 The equivalent Linux task state is `EXIT_ZOMBIE`, `EXIT_DEAD`, or `TASK_DEAD`.

</td></tr><tbody></table>
### psinfo\_t {#dt_ref_procpsinfo_prov}

Several `proc` probes have an argument of type `psinfo_t`. Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/procfs.d`. The definition of the `psinfo_t` structure, is as follows:

```nocopybutton
typedef struct psinfo {
        int pr_flag;                    /* process flags (DEPRECATED) */
        int pr_nlwp;                    /* number of active lwps (Linux: 1) */
        pid_t pr_pid;                   /* unique process id */
        pid_t pr_ppid;                  /* process id of parent */
        pid_t pr_pgid;                  /* pid of process group leader */
        pid_t pr_sid;                   /* session id */
        uid_t pr_uid;                   /* real user id */
        uid_t pr_euid;                  /* effective user id */
        uid_t pr_gid;                   /* real group id */
        uid_t pr_egid;                  /* effective group id */
        uintptr_t pr_addr;              /* address of process */
        size_t pr_size;                 /* size of process image (in KB) */
        size_t pr_rssize;               /* resident set sie (in KB) */
        size_t pr_pad1;
        struct tty_struct *pr_ttydev;   /* controlling tty (or -1) */
        ushort_t pr_pctcpu;             /* % of recent cpu time used */
        ushort_t pr_pctmem;             /* % of recent memory used */
        timestruc_t pr_start;           /* process start time */
        timestruc_t pr_time;            /* usr+sys cpu time for process */
        timestruc_t pr_ctime;           /* usr+sys cpu time for children */
        char pr_fname[16];              /* name of exec'd file */
        char pr_psargs[80];             /* initial chars of arg list */
        int pr_wstat;                   /* if zombie, wait() status */
        int pr_argc;                    /* initial argument count */
        uintptr_t pr_argv;              /* address of initial arg vector */
        uintptr_t pr_envp;              /* address of initial env vector */
        char pr_dmodel;                 /* data model */
        char pr_pad2[3];
        taskid_t pr_taskid;             /* task id */
        dprojid_t pr_projid;            /* project id */
        int pr_nzomb;                   /* number of zombie lwps (Linux: 0) */
        poolid_t pr_poolid;             /* pool id */
        zoneid_t pr_zoneid;             /* zone id */
        id_t pr_contract;               /* process contract */
        int pr_filler[1];
        lwpsinfo_t pr_lwp;

} psinfo_t;
```

**Note:**

Lightweight processes don't exist in Linux. In Linux, processes and threads are represented by process descriptors of type `struct task_struct` in the task list. DTrace translates the members of `psinfo_t` from the `task_struct` for the Linux process.

`pr_dmodel` is set to either `PR_MODEL_ILP32`, denoting a 32–bit process, or `PR_MODEL_LP64`, denoting a 64–bit process.

## proc Examples {#dt_ref_procexamples_prov}

The following examples illustrate the use of the probes that are published by the `proc` provider.

### create

The following example shows how you can use the `create` probe to show the pids that are creating other pids. Both the creating and resulting pids are shown in the output. Add the following D source code and save it in a file named `create.d`:

```
proc:::create
{
  printf("%d created %d\n",
      args[0]->pr_ppid,
      args[0]->pr_pid);
}
```

Run the D script. The D script shows output similar to the following:

```
CPU     ID                    FUNCTION:NAME
  0 111864                          :create 2670 created 691647

  2 111864                          :create 1 created 691648

  2 111864                          :create 691074 created 691649

  3 111864                          :create 691649 created 691650

  3 111864                          :create 691649 created 691651
...
```

### exec, exec-success and exec-failure

The following example shows how you can use the `exec`, `exec-success` and `exec-failure` probes to easily determine which programs are being run, and by which parent process. Type the following D source code and save it in a file named `whoexec.d`:

```
#pragma D option quiet

proc:::exec
{
  self->parent = execname;
}

proc:::exec-success
/self->parent != NULL/
{
  @[self->parent, execname] = count();
  self->parent = NULL;
}

proc:::exec-failure
/self->parent != NULL/
{
  self->parent = NULL;
}

END
{
  printf("%-20s %-20s %s\n", "WHO", "WHAT", "COUNT");
  printa("%-20s %-20s %@d\n", @);
}
```

Running the example script for a short period results in output similar to the following:

```nocopybutton
WHO                  WHAT                 COUNT
bash                 date                 1
bash                 grep                 1
bash                 ssh                  1
bash                 wc                   1
bash                 ls                   2
bash                 sed                  2
...
```

### start and exit

To determine how long programs are running, from creation to termination, you can enable the `start` and `exit` probes, as shown in the following example. Save it in a file named `progtime.d`:

```
proc:::start
{
  self->start = timestamp;
}

proc:::exit
/self->start/
{
  @[execname] = quantize(timestamp - self->start);
  self->start = 0;
}
```

Running the example script on a build server for several seconds results in output similar to the following:

```nocopybutton
...
cc
          value  ------------- Distribution ------------- count
       33554432 |                                         0
       67108864 |@@@                                      3
      134217728 |@                                        1
      268435456 |                                         0
      536870912 |@@@@                                     4
     1073741824 |@@@@@@@@@@@@@@                           13
     2147483648 |@@@@@@@@@@@@                             11
     4294967296 |@@@                                      3
     8589934592 |                                         0

sh
          value  ------------- Distribution ------------- count
         262144 |                                         0
         524288 |@                                        5
        1048576 |@@@@@@@                                  29
        2097152 |                                         0
        4194304 |                                         0
        8388608 |@@@                                      12
       16777216 |@@                                       9
       33554432 |@@                                       9
       67108864 |@@                                       8
      134217728 |@                                        7
      268435456 |@@@@@                                    20
      536870912 |@@@@@@                                   26
     1073741824 |@@@                                      14
     2147483648 |@@                                       11
     4294967296 |                                         3
     8589934592 |                                         1
    17179869184 |                                         0
...
```

### signal-send

The following example shows how you can use the `signal-send` probe to determine the sending and receiving of process associated with any signal. Type the following D source code and save it in a file named `sig.d`:

```
#pragma D option quiet

proc:::signal-send
{
  @[execname, stringof(args[1]->pr_fname), args[2]] = count();
}

END
{
  printf("%20s %20s %12s %s\n",
      "SENDER", "RECIPIENT", "SIG", "COUNT");
  printa("%20s %20s %12d %@d\n", @);
}
```

Running this script results in output similar to the following:

```nocopybutton
              SENDER            RECIPIENT          SIG COUNT
       kworker/u16:7               dtrace            2 1
       kworker/u16:7                 sudo            2 1
           swapper/2             sssd_kcm           34 1
           swapper/6             pmlogger           14 1
```

## proc Stability {#dt_ref_procstab_prov}

The `proc` provider uses DTrace's stability mechanism to describe its stabilities. These values are listed in the following table.

<table><thead><tr><th>

Element

</th><th>

Name Stability

</th><th>

Data Stability

</th><th>

Dependency Class

</th></tr></thead><tbody><tr><td>

Provider

</td><td>

Evolving

</td><td>

Evolving

</td><td>

ISA

</td></tr><tr><td>

Module

</td><td>

Private

</td><td>

Private

</td><td>

Unknown

</td></tr><tr><td>

Function

</td><td>

Private

</td><td>

Private

</td><td>

Unknown

</td></tr><tr><td>

Name

</td><td>

Evolving

</td><td>

Evolving

</td><td>

ISA

</td></tr><tr><td>

Arguments

</td><td>

Evolving

</td><td>

Evolving

</td><td>

ISA

</td></tr><tbody></table>
