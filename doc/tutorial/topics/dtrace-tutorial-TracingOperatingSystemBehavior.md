<!--
 SPDX-FileCopyrightText: 2013,2024 Oracle and/or its affiliates.
 SPDX-License-Identifier: CC-BY-SA-4.0
-->

---
publisherinformation: September 2022
---

# Tracing Operating System Behavior

This chapter provides examples of D programs that you can use to investigate what is happening in the operating system.

## Tracing Process Creation

The `proc` probes enable you to trace process creation and termination, execution of new program images, and signal processing on a system. See [proc Provider](https://docs.oracle.com/en/operating-systems/oracle-linux/dtrace-v2-guide/dtrace-ref-procProvider.html) for a description of the `proc` probes and their arguments.

### Example: Monitoring the System as Programs Are Executed \(execcalls.d\) 

The following example shows the D program, `execcalls.d`, which uses `proc` probes to monitor the system as it executes process images:

```
/* execcalls.d -- Monitor the system as it executes programs */

proc::do_execveat_common:exec
{
  trace(stringof(args[0]));
}
```

The `args[0]` argument to the `exec` probe is set to the path name of the program that is being executed. You use the `stringof()` function to convert the type from `char *` to the D type `string`.

**Note:**

The `sdt` kernel module, which enables the `proc` provider probes, is most likely already loaded on the test system. Or, if not already loaded, the `sdt` kernel module will automatically load if you did not manually load a DTrace module since booting the system. See [Manually Loading DTrace Modules](dtrace-tutorial-IntroducingDTrace.md#) for details. In the following example, the `sdt` kernel module needs to be manually loaded or it must be able to automatically load for proper functionality.

Type the `dtrace -s execcalls.d` command to run the D program in one window. Then start different programs from another window, while observing the output from `dtrace` in the first window. To stop tracing after a few seconds have elapsed, type `Ctrl-C` in the window that is running `dtrace`.

```
# `dtrace -s execcalls.d`
dtrace: script 'execcalls.d' matched 1 probe
CPU     ID                    FUNCTION:NAME
  1   1185          do_execveat_common:exec /usr/sbin/sshd
  0   1185          do_execveat_common:exec /usr/sbin/unix_chkpwd
  0   1185          do_execveat_common:exec /bin/bash
  0   1185          do_execveat_common:exec /usr/bin/id
  0   1185          do_execveat_common:exec /usr/bin/hostname
  0   1185          do_execveat_common:exec /usr/bin/id
  0   1185          do_execveat_common:exec /usr/bin/id
  0   1185          do_execveat_common:exec /usr/bin/grep
  0   1185          do_execveat_common:exec /usr/bin/tty
  0   1185          do_execveat_common:exec /usr/bin/tput
  0   1185          do_execveat_common:exec /usr/bin/grep
  1   1185          do_execveat_common:exec /usr/sbin/unix_chkpwd
  1   1185          do_execveat_common:exec /usr/libexec/grepconf.sh
  1   1185          do_execveat_common:exec /usr/bin/dircolors
  0   1185          do_execveat_common:exec /usr/bin/ls`
^C`
```

The activity here shows a login to the same system \(from another terminal\) while the script is running.

The probe `proc::do_execveat_common:exec` fires whenever the system executes a new program and the associated action uses `trace()` to display the path name of the program.

### Exercise: Suppressing Verbose Output From DTrace

Run the `execcalls.d` program again, but this time add the `-q` option to suppress all output except output from `trace()`. Notice how DTrace displays only what you traced with `trace()`.

\(Estimated completion time: less than 5 minutes\)

### Solution to Exercise: Suppressing Verbose Output From DTrace 

```
# `dtrace -q -s execcalls.d`
/usr/bin/id/usr/bin/tput/usr/bin/dircolors/usr/bin/id/
usr/lib64/qt-3.3/bin/gnome-terminal/usr/local/bin/gnome-terminal
/usr/bin/gnome-terminal/bin/bash/usr/bin/id/bin/grep/bin/basename
/usr/bin/tty/bin/ps
```

## Tracing System Calls

System calls are the interface between user programs and the kernel, which perform operations on the programs' behalf.

The next example shows the next D program, `syscalls.d`, which uses `syscall` probes to record `open()` system call activity on a system.

### Example: Recording open\(\) System Calls on a System \(syscalls.d\) 

```
/* syscalls.d -- Record open() system calls on a system */

syscall::open:entry
{
  printf("%-16s %-16s\n",execname,copyinstr(arg0));
}
```

In this example, the `printf()` function is used to display the name of the executable that is calling `open()` and the path name of the file that it is attempting to open.

**Note:**

Use the `copyinstr()` function to convert the first argument \(`arg0`\) in the `open()` call to a string. Whenever a probe accesses a pointer to data in the address space of a user process, you must use one of the `copyin()`, `copyinstr()`, or `copyinto()` functions to copy the data from user space to a DTrace buffer in kernel space. In this example, it is appropriate to use `copyinstr()`, as the pointer refers to a character array. If the string is not null-terminated, you also need to specify the length of the string to `copyinstr()`, for example, `copyinstr(arg1, arg2)`, for a system call such as `write()`. 

The `sdt` kernel module, which enables the `proc` provider probes, is most likely already loaded on the test system. Or, if not already loaded, the `sdt` kernel module will automatically load if you did not manually load a DTrace module since booting the system. See [Manually Loading DTrace Modules](dtrace-tutorial-IntroducingDTrace.md#) for details.

In the following example, the `sdt` kernel module needs to be manually loaded or it must be able to automatically load for proper functionality:

```
# `dtrace -q -s syscalls.d`
udisks-daemon    /dev/sr0               
devkit-power-da  /sys/devices/LNXSYSTM:00/.../PNP0C0A:00/power_supply/BAT0/present
devkit-power-da  /sys/devices/LNXSYSTM:00/.../PNP0C0A:00/power_supply/BAT0/energy_now
devkit-power-da  /sys/devices/LNXSYSTM:00/.../PNP0C0A:00/power_supply/BAT0/voltage_max_design
devkit-power-da  /sys/devices/LNXSYSTM:00/.../PNP0C0A:00/power_supply/BAT0/voltage_min_design
devkit-power-da  /sys/devices/LNXSYSTM:00/.../PNP0C0A:00/power_supply/BAT0/status
devkit-power-da  /sys/devices/LNXSYSTM:00/.../PNP0C0A:00/power_supply/BAT0/current_now
devkit-power-da  /sys/devices/LNXSYSTM:00/.../PNP0C0A:00/power_supply/BAT0/voltage_now     
VBoxService      /var/run/utmp         
firefox          /home/guest/.mozilla/firefox/qeaojiol.default/sessionstore.js
firefox          /home/guest/.mozilla/firefox/qeaojiol.default/sessionstore-1.js
firefox          /home/guest/.mozilla/firefox/qeaojiol.default/sessionstore-1.js    
`^C`
```

### Exercise: Using the printf\(\) Function to Format Output

Amend the arguments to the `printf()` function so that `dtrace` also prints the process ID and user ID for the process. Use a conversion specifier such as `%-4d`.

See the [DTrace Function Reference](https://docs.oracle.com/en/operating-systems/oracle-linux/dtrace-v2-guide/dtrace_functions.html) for a description of the `printf()` function.

The process ID and user ID are available as the variables `pid` and `uid`. Use the `BEGIN` probe to create a header for the output.

\(Estimated completion time: 10 minutes\)

### Solution to Exercise: Using the printf\(\) Function to Format Output

```
/* syscalls1.d -- Modified version of syscalls.d that displays more information */

BEGIN
{
  printf("%-6s %-4s %-16s %-16s\n","PID","UID","EXECNAME","FILENAME");
}

syscall::open:entry
{
  printf("%-6d %-4d %-16s %-16s\n",pid,uid, execname,copyinstr(arg0));
}
```

Note how this solution uses similar formatting strings to output the header and the data.

```
# `dtrace -q -s syscalls1.d`
PID    UID  EXECNAME         FILENAME
3220   0    udisks-daemon    /dev/sr0        
2571   0    sendmail         /proc/loadavg   
3220   0    udisks-daemon    /dev/sr0        
2231   4    usb              /dev/usblp0     
2231   4    usb              /dev/usb/lp0    
2231   4    usb              /dev/usb/usblp0
...
`^C`
```

## Performing an Action at Specified Intervals

The `profile` provider includes `tick` probes that you can use to sample some aspect of a system's state at regular intervals. Note that the `profile` kernel module must be loaded to use these probes.

### Example: Using `tick.d`



The following is an example of the `tick.d` program.

```
/* tick.d -- Perform an action at regular intervals */

BEGIN
{
  i = 0;
}

profile:::tick-1sec
{
  printf("i = %d\n",++i);
}

END
{
  trace(i);
}
```

In this example, the program declares and initializes the variable `i` when the D program starts, increments the variable and prints its value once every second, and displays the final value of `i` when the program exits.

When you run this program, it produces output that is similar to the following, until you type `Ctrl-C`:

```
# `dtrace -s tick.d` 
dtrace: script 'tick.d' matched 3 probes
CPU     ID                    FUNCTION:NAME
  1   5315                       :tick-1sec i = 1

  1   5315                       :tick-1sec i = 2

  1   5315                       :tick-1sec i = 3

  1   5315                       :tick-1sec i = 4

  1   5315                       :tick-1sec i = 5

  1   5315                       :tick-1sec i = 6
`
^C`
  1   5315                       :tick-1sec i = 7

  0      2                             :END         7 
```

To suppress all of the output except the output from `printf()` and `trace()`, specify the `-q` option:

```
# `dtrace -q -s tick.d` 
i = 1
i = 2
i = 3
i = 4
`^C`
i = 5
5
```

### Exercise: Using tick Probes

List the available `profile` provider probes. Experiment with using a different `tick` probe. Replace the `trace()` call in `END` with a `printf()` call.

See [profile Provider](https://docs.oracle.com/en/operating-systems/oracle-linux/dtrace-v2-guide/dtrace_providers_profile.html) for a description of the probes.

\(Estimated completion time: 10 minutes\)

### Solution to Exercise and Example: Using tick Probes 

```
# `dtrace -l -P profile`
  ID   PROVIDER           MODULE               FUNCTION NAME
   5    profile                                         profile-97
   6    profile                                         profile-199
   7    profile                                         profile-499
   8    profile                                         profile-997
   9    profile                                         profile-1999
  10    profile                                         profile-4001
  11    profile                                         profile-4999
  12    profile                                         tick-1
  13    profile                                         tick-10
  14    profile                                         tick-100
  15    profile                                         tick-500
  16    profile                                         tick-1000
  17    profile                                         tick-5000
5315    profile                                         tick-1sec
5316    profile                                         tick-10sec
```

### Example: Modified Version of tick.d 

```
/* tick1.d -- Modified version of tick.d */

BEGIN
{
  i = 0;
}

/* tick-500ms fires every 500 milliseconds */
profile:::tick-500ms
{
  printf("i = %d\n",++i);
}

END
{
  printf("\nFinal value of i = %d\n",i);
}
```

This example uses the `tick-500ms` probe, which fires twice per second.

```
# `dtrace -s tick1.d`
dtrace: script 'tick1.d' matched 3 probes
CPU     ID                    FUNCTION:NAME
  2    642                      :tick-500ms i = 1

  2    642                      :tick-500ms i = 2

  2    642                      :tick-500ms i = 3

  2    642                      :tick-500ms i = 4

`^C`
  2    642                      :tick-500ms i = 5

  3      2                             :END
Final value of i = 5
```

## Using Predicates to Select Actions

Predicates are logic statements that choose whether DTrace invokes the actions that are associated with a probe. You can use predicates to focus tracing analysis on specific contexts under which a probe fires.

### Example: Using daterun.d

The following example shows an executable DTrace script, `daterun.d`, which displays the file descriptor, output string, and string length specified to the `write()` system call whenever the `date` command is run on the system.

```
#!/usr/sbin/dtrace -qs

/* daterun.d -- Display arguments to write() when date runs */

syscall::write:entry
/execname == "date"/
{
  printf("%s(%d, %s, %d)\n", probefunc, arg0, copyinstr(arg1), arg2);
} 
```

In the example, the predicate is `/execname == "date"/`, which specifies that if the probe `syscall::write:entry` is triggered, DTrace runs the associated action only if the name of the executable is `date`.

Make the script executable by changing its mode:

```
# `chmod +x daterun.d`
```

If you run the script from one window, while typing the `date` command in another window, output similar to the following is displayed in the first window:

```
# `./daterun.d`
write(1, Thu Oct 31 11:14:43 GMT 2013
, 29)
```

### Example: Listing Available syscall Provider Probes 

The following example shows how you would list available `syscall` provider probes.

```
# `dtrace -l -P syscall | less`
   ID   PROVIDER            MODULE                 FUNCTION NAME
   18    syscall           vmlinux                     read entry
   19    syscall           vmlinux                     read return
   20    syscall           vmlinux                    write entry
   21    syscall           vmlinux                    write return
   22    syscall           vmlinux                     open entry
   23    syscall           vmlinux                     open return
   24    syscall           vmlinux                    close entry
   25    syscall           vmlinux                    close return
   26    syscall           vmlinux                  newstat entry
   27    syscall           vmlinux                  newstat return
...
  648    syscall           vmlinux               pkey_alloc entry
  649    syscall           vmlinux               pkey_alloc return
  650    syscall           vmlinux                pkey_free entry
  651    syscall           vmlinux                pkey_free return
  652    syscall           vmlinux                    statx entry
  653    syscall           vmlinux                    statx return
  654    syscall           vmlinux                   waitfd entry
  655    syscall           vmlinux                   waitfd return
```

### Exercise: Using syscall Probes

Experiment by adapting the `daterun.d` script for another program. Make the new script produce output when the system is running `w`.

\(Estimated completion time: 10 minutes\)

### Solution to Exercise: Using syscall Probes 

```
#!/usr/sbin/dtrace -qs

/* wrun.d -- Modified version of daterun.d for the w command */

syscall::write:entry
/execname == "w"/
{
  printf("%s(%d, %s, %d)\n", probefunc, arg0, copyinstr(arg1, arg2), arg2);
} 
```

The program uses the two-argument form of `copyinstr()`, as the string argument to `write()` might not be null-terminated:

```
# `chmod +x wrun.d`
# `./wrun.d`
write(1,  12:14:55 up  3:21,  3 users,  load average: 0.14, 0.15, 0.18
, 62)
write(1, USER     TTY      FROM              LOGIN@   IDLE   JCPU   PCPU WHAT
, 69)
write(1, guest    tty1     :0               08:55    3:20m 11:23   0.17s pam: gdm-passwo
, 80)
write(1, guest    pts/0    :0.0             08:57    7.00s  0.17s  0.03s w
m: gdm-passwo
, 66)
write(1, guest    pts/1    :0.0             12:14    7.00s  0.69s  8.65s gnome-terminal

, 79)
...
^C
```

## Timing Events on a System

Determining the time that a system takes to perform different activities is a fundamental technique for analyzing its operation and determining where bottlenecks might be occurring.

### Example: Monitoring read\(\) System Call Duration \(readtrace.d\) 

The following is an example of the D program, `readtrace.d`.

```
/* readtrace.d -- Display time spent in read() calls */

syscall::read:entry
{
  self->t = timestamp; /* Initialize a thread-local variable */
}

syscall::read:return
/self->t != 0/
{
  printf("%s (pid=%d) spent %d microseconds in read()\n",
  execname, pid, ((timestamp - self->t)/1000)); /* Divide by 1000 for microseconds */

  self->t = 0; /* Reset the variable */
}
```

In the example, the `readtrace.d` program displays the command name, process ID, and call duration in microseconds whenever a process invokes the `read()` system call. The variable `self->t` is *thread-local*, meaning that it exists only within the scope of execution of a thread on the system. The program records the value of `timestamp` in `self->t` when the process calls `read()`, and subtracts this value from the value of `timestamp` when the call returns. The units of `timestamp` are nanoseconds, so you divide by 1000 to obtain a value in microseconds.

The following is output from running this program:

```
# `dtrace -q -s readtrace.d`
NetworkManager (pid=878) spent 10 microseconds in read()
NetworkManager (pid=878) spent 9 microseconds in read()
NetworkManager (pid=878) spent 2 microseconds in read()
in:imjournal (pid=815) spent 63 microseconds in read()
gdbus (pid=878) spent 7 microseconds in read()
gdbus (pid=878) spent 66 microseconds in read()
gdbus (pid=878) spent 63 microseconds in read()
irqbalance (pid=816) spent 56 microseconds in read()
irqbalance (pid=816) spent 113 microseconds in read()
irqbalance (pid=816) spent 104 microseconds in read()
irqbalance (pid=816) spent 91 microseconds in read()
irqbalance (pid=816) spent 61 microseconds in read()
irqbalance (pid=816) spent 63 microseconds in read()
irqbalance (pid=816) spent 61 microseconds in read()
irqbalance (pid=816) spent 61 microseconds in read()
irqbalance (pid=816) spent 61 microseconds in read()
irqbalance (pid=816) spent 61 microseconds in read()
irqbalance (pid=816) spent 61 microseconds in read()
irqbalance (pid=816) spent 61 microseconds in read()
sshd (pid=10230) spent 8 microseconds in read()
in:imjournal (pid=815) spent 6 microseconds in read()
sshd (pid=10230) spent 7 microseconds in read()
in:imjournal (pid=815) spent 5 microseconds in read()
sshd (pid=10230) spent 7 microseconds in read()
in:imjournal (pid=815) spent 6 microseconds in read()
sshd (pid=10230) spent 7 microseconds in read()
in:imjournal (pid=815) spent 5 microseconds in read()
`^C`
```

### Exercise: Timing System Calls

Add a predicate to the `entry` probe in `readtrace.d` so that `dtrace` displays results for a disk space usage report that is selected by the name of its executable \(`df`\).

\(Estimated completion time: 10 minutes\)

### Solution to Exercise: Timing System Calls 

The following example shows a modified version of the `readtrace.d` program that includes a predicate.

```
/* readtrace1.d -- Modified version of readtrace.d that includes a predicate */

syscall::read:entry
/execname == "df"/
{
  self->t = timestamp;
}

syscall::read:return
/self->t != 0/
{
  printf("%s (pid=%d) spent %d microseconds in read()\n",
  execname, pid, ((timestamp - self->t)/1000));

  self->t = 0; /* Reset the variable */
}
```

The predicate `/execname == "df"/` tests whether the `df` program is running when the probe fires.

```
# `dtrace -q -s readtrace1.d` 
df (pid=1666) spent 6 microseconds in read()
df (pid=1666) spent 8 microseconds in read()
df (pid=1666) spent 1 microseconds in read()
df (pid=1666) spent 50 microseconds in read()
df (pid=1666) spent 38 microseconds in read()
df (pid=1666) spent 10 microseconds in read()
df (pid=1666) spent 1 microseconds in read()
`^C`
```

### Exercise: Timing All System Calls for cp \(calltrace.d\) 

Using the `probefunc` variable and the `syscall:::entry` and `syscall:::return` probes, create a D program, `calltrace.d`, which times all system calls for the executable `cp`.

\(Estimated completion time: 10 minutes\)

### Solution to Exercise: Timing All System Calls for cp \(calltrace.d\) 

```
/* calltrace.d -- Time all system calls for cp */

syscall:::entry
/execname == "cp"/
{
  self->t = timestamp; /* Initialize a thread-local variable */
}

syscall:::return
/self->t != 0/
{
  printf("%s (pid=%d) spent %d microseconds in %s()\n",
  execname, pid, ((timestamp - self->t)/1000), probefunc);

  self->t = 0; /* Reset the variable */
}
```

Dropping the function name `read` from the probe specifications matches all instances of `entry` and `return` probes for `syscall`. The following is a check for system calls resulting from running the `cp` executable:

```
# `dtrace -q -s calltrace.d` 
cp (pid=2801) spent 4 microseconds in brk()
cp (pid=2801) spent 5 microseconds in mmap()
cp (pid=2801) spent 15 microseconds in access()
cp (pid=2801) spent 7 microseconds in open()
cp (pid=2801) spent 2 microseconds in newfstat()
cp (pid=2801) spent 3 microseconds in mmap()
cp (pid=2801) spent 1 microseconds in close()
cp (pid=2801) spent 8 microseconds in open()
cp (pid=2801) spent 3 microseconds in read()
cp (pid=2801) spent 1 microseconds in newfstat()
cp (pid=2801) spent 4 microseconds in mmap()
cp (pid=2801) spent 12 microseconds in mprotect()
   ...
cp (pid=2801) spent 183 microseconds in open()
cp (pid=2801) spent 1 microseconds in newfstat()
cp (pid=2801) spent 1 microseconds in fadvise64()
cp (pid=2801) spent 17251 microseconds in read()
cp (pid=2801) spent 80 microseconds in write()
cp (pid=2801) spent 58 microseconds in read()
cp (pid=2801) spent 57 microseconds in close()
cp (pid=2801) spent 85 microseconds in close()
cp (pid=2801) spent 57 microseconds in lseek()
cp (pid=2801) spent 56 microseconds in close()
cp (pid=2801) spent 56 microseconds in close()
cp (pid=2801) spent 56 microseconds in close()`
^C`
```

## Tracing Parent and Child Processes

When a process forks, it creates a child process that is effectively a copy of its parent process, but with a different process ID. For information about other differences, see the `fork(2)` manual page. The child process can either run independently from its parent process to perform some separate task. Or, a child process can execute a new program image that replaces the child's program image while retaining the same process ID.

### Example: Using proc Probes to Report Activity on a System \(activity.d\) 

The D program `activity.d` in the following example uses `proc` probes to report `fork()` and `exec()` activity on a system.

```
#pragma D option quiet

/* activity.d -- Record fork() and exec() activity */

proc::_do_fork:create
{
  /* Extract PID of child process from the psinfo_t pointed to by args[0] */
  childpid = args[0]->pr_pid;

  time[childpid] = timestamp;
  p_pid[childpid] = pid; /* Current process ID (parent PID of new child) */
  p_name[childpid] = execname; /* Parent command name */
  p_exec[childpid] = ""; /* Child has not yet been exec'ed */ 
}

proc::do_execveat_common:exec
/p_pid[pid] != 0/
{
  p_exec[pid] = args[0]; /* Child process path name */
}

proc::do_exit:exit
/p_pid[pid] != 0 &&  p_exec[pid] != ""/ 
{
  printf("%s (%d) executed %s (%d) for %d microseconds\n",
    p_name[pid], p_pid[pid], p_exec[pid], pid, (timestamp - time[pid])/1000);
}

proc::do_exit:exit
/p_pid[pid] != 0 &&  p_exec[pid] == ""/
{
  printf("%s (%d) forked itself (as %d) for %d microseconds\n",
    p_name[pid], p_pid[pid], pid, (timestamp - time[pid])/1000);
}  
```

In the example, the statement `#pragma D option quiet` has the same effect as specifying the `-q` option on the command line.

The process ID of the child process \(`childpid`\), following a `fork()`, is determined by examining the `pr_pid` member of the `psinfo_t` data structure that is pointed to by the `args[0]` probe argument. For more information about the arguments to `proc` probes, see [proc Provider](https://docs.oracle.com/en/operating-systems/oracle-linux/dtrace-v2-guide/dtrace-ref-procProvider.html).

The program uses the value of the child process ID to initialize globally unique associative array entries, such as `p_pid[childpid]`.

**Note:**

An *associative array* is similar to a normal array, in that it associates keys with values, but the keys can be of any type; they need not be integers.

When you run the program, you should see output similar to the following as you use the `ssh` command to access the same system from another terminal window. You might want to try running different programs from this new terminal window to generate additional output:

```
# `dtrace -s activity.d` 
sshd (3966) forked itself (as 3967) for 3667020 microseconds
bash (3971) forked itself (as 3972) for 1718 microseconds
bash (3973) executed /usr/bin/hostname (3974) for 1169 microseconds
grepconf.sh (3975) forked itself (as 3976) for 1333 microseconds
bash (3977) forked itself (as 3978) for 967 microseconds
bash (3977) executed /usr/bin/tput (3979) for 1355 microseconds
bash (3980) executed /usr/bin/dircolors (3981) for 1212 microseconds
sshd (3966) executed /usr/sbin/unix_chkpwd (3968) for 31444 microseconds
sshd (3966) executed /usr/sbin/unix_chkpwd (3969) for 1653 microseconds
bash (3970) forked itself (as 3971) for 2411 microseconds
bash (3970) forked itself (as 3973) for 1830 microseconds
bash (3970) executed /usr/libexec/grepconf.sh (3975) for 3696 microseconds
bash (3970) forked itself (as 3977) for 3273 microseconds
bash (3970) forked itself (as 3980) for 1928 microseconds
bash (3970) executed /usr/bin/grep (3982) for 1570 microseconds
`^C`
```

### Exercise: Using a Predicate to Control the Execution of an Action

Modify `activity.d` so that `dtrace` displays results for parent processes that are selected by their executable name, for example, `bash`, or by a program name that you specify as an argument to the `dtrace` command.

\(Estimated completion time: 10 minutes\)

### Solution to Exercise: Using a Predicate to Control the Execution of an Action



The only change that is required to specify the name of an executable is to add a predicate to the `proc::_do_fork:create` probe, for example:

```
/execname == "bash"/
```

A more generic version of the program sets the predicate check value from a passed-in command-line argument instead, for example:

```
/execname == $1/
```

### Example: Recording fork\(\) and exec\(\) Activity for a Specified Program \(activity1.d\)

The following example uses a predicate that is passed in from the command line.

```
#pragma D option quiet

/* activity1.d -- Record fork() and exec() activity for a specified program */

proc::_do_fork:create
/execname == $1/
{
  /* Extract PID of child process from the psinfo_t pointed to by args[0] */
  childpid = args[0]->pr_pid;

  time[childpid] = timestamp;
  p_pid[childpid] = pid; /* Current process ID (parent PID of new child) */
  p_name[childpid] = execname; /* Parent command name */
  p_exec[childpid] = ""; /* Child has not yet been exec'ed */ 
}

proc::do_execveat_common:exec
/p_pid[pid] != 0/
{
  p_exec[pid] = args[0]; /* Child process path name */
}

proc::do_exit:exit
/p_pid[pid] != 0 &&  p_exec[pid] != ""/ 
{
  printf("%s (%d) executed %s (%d) for %d microseconds\n",
    p_name[pid], p_pid[pid], p_exec[pid], pid, (timestamp - time[pid])/1000);
}

proc::do_exit:exit
/p_pid[pid] != 0 &&  p_exec[pid] == ""/
{
  printf("%s (%d) forked itself (as %d) for %d microseconds\n",
    p_name[pid], p_pid[pid], pid, (timestamp - time[pid])/1000);
}  
```

As shown in the following example, you can now specify the name of the program to be traced as an argument to the `dtrace` command. Note that you need to escape the argument to protect the double quotes from the shell:

```
# `dtrace -s activity.d '"bash"'`
bash (10367) executed /bin/ps (10368) for 10926 microseconds
bash (10360) executed /usr/bin/tty (10361) for 3046 microseconds
bash (10359) forked itself (as 10363) for 32005 microseconds
bash (10366) executed /bin/basename (10369) for 1285 microseconds
bash (10359) forked itself (as 10370) for 12373 microseconds
bash (10360) executed /usr/bin/tput (10362) for 34409 microseconds
bash (10363) executed /usr/bin/dircolors (10364) for 29527 microseconds
bash (10359) executed /bin/grep (10365) for 21024 microseconds
bash (10366) forked itself (as 10367) for 11749 microseconds
bash (10359) forked itself (as 10360) for 41918 microseconds
bash (10359) forked itself (as 10366) for 14197 microseconds
bash (10370) executed /usr/bin/id (10371) for 11729 microseconds
`^C`
```

## Simple Data Aggregations

DTrace provides several functions for aggregating the data that individual probes gather. These functions include `avg()`, `count()`, `max()`, `min()`, `stddev()`, and `sum()`, which return the mean, number, maximum value, minimum value, standard deviation, and summation of the data being gathered, respectively. See [Aggregations](https://docs.oracle.com/en/operating-systems/oracle-linux/dtrace-v2-guide/d_program_syntax_reference.html#reference_yk3_cqp_pwb) for descriptions of aggregation functions.

DTrace indexes the results of an aggregation by using a tuple expression that similar to what is used for an associative array:

```
@*name*[*list\_of\_keys*] = *aggregating\_function*(*args*);
```

The name of the aggregation is prefixed with an `@` character. The keys describe the data that the aggregating function is collecting. If you do not specify a name for the aggregation, DTrace uses `@` as an anonymous aggregation name, which is usually sufficient for simple D programs.

### Example: Counting the Number of write\(\) System Calls Invoked by Processes

In the following example, the command counts the number of `write()` system calls that are invoked by processes, until you type `Ctrl-C`.

```
# `dtrace -n 'syscall::write:entry { @["write() calls"] = count(); }'`
dtrace: description 'syscall:::' matched 1 probe
`^C`

  write() calls                                              9
```

**Note:**

Rather than create a separate D script for this simple example, the probe and the action is specified on the `dtrace` command line.

DTrace prints the result of the aggregation automatically. Alternatively, you can use the `printa()` function to format the result of the aggregation.

### Example: Counting the Number of read\(\) and write\(\) System Calls

The following example counts the number of both `read()` and `write()` system calls.

```
# `dtrace -n 'syscall::write:entry,syscall::read:entry { @[strjoin(probefunc,"() calls")] = count(); }'` 
dtrace: description 'syscall::write:entry,syscall::read:entry' matched 2 probes
`^C`

  write() calls                                            150
  read() calls                                            1555
```

### Exercise: Counting System Calls Over a Fixed Period

Write a D program named `countcalls.d` that uses a `tick` probe and `exit()` to stop collecting data after 100 seconds and display the number of `open()`, `read()` and `write()` calls.

\(Estimated completion time: 15 minutes\)

### Solution to Exercise and Example: Counting Write, Read, and Open System Calls Over 100 Seconds \(countcalls.d\)  

```
/* countcalls.d -- Count write, read, and open system calls over 100 seconds */

profile:::tick-100sec
{
  exit(0);
}

syscall::write:entry, syscall::read:entry, syscall::open:entry
{
  @[strjoin(probefunc,"() calls")] = count();
}
```

The action that is associated with the `tick-100s` probe means that `dtrace` exits after 100 seconds and prints the results of the aggregation.

```
# `dtrace -s countcalls.d`
dtrace: script 'countcalls.d' matched 4 probes
CPU     ID                    FUNCTION:NAME
  3    643                     :tick-100sec 

  write() calls                                                  1062
  open() calls                                                   1672
  read() calls                                                  29672
```

### Example: Counting System Calls Invoked by a Process \(countsyscalls.d\) 

The D program `countsyscalls.d` shown in the following example counts the number of times a process that is specified by its process ID invokes different system calls.

```
#!/usr/sbin/dtrace -qs

/* countsyscalls.d -- Count system calls invoked by a process */

syscall:::entry
/pid == $1/
{
  @num[probefunc] = count();
}
```

After making the `syscalls.d` file executable, you can run it from the command line, specifying a process ID as its argument.

The following example shows how you would monitor the use of the `emacs` program that was previously invoked. After the script is invoked, within `emacs` a couple files are opened, modified, and then saved before exiting the D script.

Make the script executable:

```
# `chmod +x countsyscalls.d`
```

From another command line, type:

```
# `emacs foobar.txt`
```

Now, start the script and use the opened `emacs` window:

```
# `./countsyscalls.d $(pgrep -u root emacs)`
                     `^C `

  chmod                                                             1 
  exit_group                                                        1 
  futex                                                             1 
  getpgrp                                                           1 
  lseek                                                             1 
  lsetxattr                                                         1 
  rename                                                            1 
  fsync                                                             2 
  lgetxattr                                                         2 
  alarm                                                             3 
  rt_sigaction                                                      3 
  unlink                                                            3 
  mmap                                                              4 
  munmap                                                            4 
  symlink                                                           4 
  fcntl                                                             6 
  newfstat                                                          6 
  getgid                                                            7 
  getuid                                                            7 
  geteuid                                                           8 
  openat                                                            8 
  access                                                            9 
  getegid                                                          14 
  open                                                             14 
  getdents                                                         15 
  close                                                            17 
  readlink                                                         19 
  newlstat                                                         33 
  newstat                                                         155 
  read                                                            216 
  timer_settime                                                   231 
  write                                                           314 
  pselect6                                                        376 
  rt_sigreturn                                                    393 
  ioctl                                                           995 
  rt_sigprocmask                                                 1261 
  clock_gettime                                                  3495 
```

In the preceding example, the `pgrep` command is used to determine the process ID of the `emacs` program that the `root` user is running.

### Exercise: Tracing Processes That Are Run by a User

Create a program `countprogs.d` that counts and displays the number of times a user \(specified by their user name\) runs different programs. You can use the `id -u *user*` command to obtain the ID that corresponds to a user name.

\(Estimated completion time: 10 minutes\)

### Solution to Exercise and Example: Counting Programs Invoked by a Specified User \(countprogs.d\)  

```
#!/usr/sbin/dtrace -qs

/* countprogs.d -- Count programs invoked by a specified user */

proc::do_execveat_common:exec
/uid == $1/
{
  @num[execname] = count();
}
```

The predicate `/uid == $1/` compares the effective UID for each program that is run against the argument specified on the command line. You can use the `id -u *user*` command to find out the ID of the guest user account, for example:

```
# `chmod +x countprogs.d`
# ./countprogs.d $(id -u guest)
^C

less 1
lesspipe.sh 1
sh 1
bash 9
```

You can use the same command for the `root` user, which is typically user `0`. For testing purposes, you might want to have the user account under a test login by using another window and then run some nominal programs.

### Example: Counting the Number of Times a Program Reads From Different Files in 10 Seconds \(fdscount.d\) 

The following D program counts the number of times a program reads from different files, within ten seconds, and displays just the top five results.

```
# `emacs fdscount.d`
# `dtrace -C -D ENAME='"emacs"' -qs fdscount.d`

  /usr/share/terminfo/x/xterm                                       2
  /dev/urandom                                                      3
  /usr/share/emacs/24.3/lisp/calendar/time-date.elc                 5
  /dev/tty                                                          8
  /usr/share/emacs/24.3/lisp/term/xterm.elc                         8
```

Use the `fds[]` built-in array to determine which file corresponds to the file descriptor argument `arg0` to `read()`. The `fi_pathname` member of the `fileinfo_t` structure that is indexed in `fds[]` by `arg0` contains the full pathname of the file.

The `trunc()` function in the `END` action instructs DTrace to display just the top five results from the aggregation.

DTrace has access to the `profile:::tick-10s` probe, the `fds[]` built-in array, and the `syscall::read:entry` probe. You specify a C preprocessor directive to `dtrace` that sets the value of the *ENAME* variable, such as to `emacs`. Although, you could choose any executable. Note that you must use additional single quotes to escape the string quotes, for example:

```
# `dtrace -C -D ENAME='"emacs"' -qs fdscount.d`

/usr/share/terminfo/x/xterm 2
/dev/tty 3
/dev/urandom 3
/usr/share/emacs/24.3/lisp/calendar/time-date.elc 5
/usr/share/emacs/24.3/lisp/term/xterm.elc 8
```

If the executable under test shows a `/proc/` *pid*/`maps` entry in the output, it refers to a file in the `procfs` file system that contains information about the process's mapped memory regions and permissions. Seeing `pipe:` *inode* and `socket:` *inode* entries would refer to inodes in the `pipefs` and `socketfs` file systems.

### Exercise: Counting Context Switches on a System

Create an executable D program named `cswpercpu.d` that displays a timestamp and prints the number of context switches per CPU and the total for all CPUs once per second, together with the CPU number or `"total"`.

-   Using the `BEGIN` probe, print a header for the display with columns labelled `Timestamp`, `CPU`, and `Ncsw`.

-   Using the `sched:::on-cpu` probe to detect the end of a context switch, use `lltostr()` to convert the CPU number for the context in which the probe fired to a string, and use `count()` to increment the aggregation variable `@n` once with the key value set to the CPU number string and once with the key value set to `"total"`.

-   Using the `profile:::tick-1sec` probe, use `printf()` to print the data and time, use `printa()` to print the key \(the CPU number string or `"total"`\) and the aggregation value. The date and time are available as the value of `walltimestamp` variable, which you can print using the `%Y` conversion format

-   Use `clear()` to reset the aggregation variable `@n`.


\(Estimated completion time: 40 minutes\)

### Solution to Exercise and Example: Counting Context Switches on a System

The following example shows the executable D program `cswpercpu.d`. The program displays a timestamp and prints the number of context switches, per-CPU, and the total for all CPUs, once per second, together with the CPU number or `"total"`:

```
#!/usr/sbin/dtrace -qs

/* cswpercpu.d -- Print number of context switches per CPU once per second */

#pragma D option quiet

dtrace:::BEGIN
{
  /* Print the header */
  printf("%-25s %5s %15s", "Timestamp", "CPU", "Ncsw");
}

sched:::on-cpu
{
  /* Convert the cpu number to a string */
  cpustr = lltostr(cpu);
  /* Increment the counters */
  @n[cpustr] = count();
  @n["total"] = count();
}

profile:::tick-1sec
{
  /* Print the date and time before the first result */
  printf("\n%-25Y ", walltimestamp);

  /* Print the aggregated counts for each CPU and the total for all CPUs */
  printa("%5s %@15d\n                          ", @n);

  /* Reset the aggregation */
  clear(@n);
}
```

```
# `chmod +x cswpercpu.d`
# `./cswpercpu.d`
Timestamp                   CPU            Ncsw
2013 Nov  6 20:47:26          1             148
                              0             155
                              3             200
                              2             272
                          total             775
                          
2013 Nov  6 20:47:27          1             348
                              0             364
                              3             364
                              2             417
                          total            1493
                          
2013 Nov  6 20:47:28          3              47
                              1             100
                              0             121
                              2             178
                          total             446
                          `^C`
```

You might want to experiment with aggregating the total time that is spent context switching and the average time per context switch. For example, you can experiment by initializing a thread-local variable to the value of `timestamp` in the action to a `sched:::off-cpu` probe, and subtracting this value from the value of `timestamp` in the action to `sched:::on-cpu`. Use the `sum()` and `avg()` aggregation functions, respectively.

## Working With More Complex Data Aggregations

Use the `lquantize()` and `quantize()` functions to display linear and power-of-two frequency distributions of data. See [Aggregations](https://docs.oracle.com/en/operating-systems/oracle-linux/dtrace-v2-guide/d_program_syntax_reference.html#reference_yk3_cqp_pwb) for a description of aggregation functions.

### Example: Displaying the Distribution of Read Sizes Resulting From a Command 

As shown in the following example, you can display the distribution of the sizes specified to `arg2` of `read()` calls that were invoked by all instances of `find` that are running. After running the script, start a search with `find` in another window, such as `find .` or `find /.`.

```
# `dtrace -n 'syscall::read:entry /execname=="find"/{@dist["find"]=quantize(arg2);}'`
dtrace: description 'syscall::read:entry ' matched 1 probe
`^C`

   find
            value  ------------- Distribution ------------- count
              256 | 0
              512 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 6
             1024 | 0
             2048 | 0
             4096 |@@@@@@@@@@ 2
             8192 | 0
```

If the program is as simple as the program in the previous example, it is often convenient to run it from the command line.

### Example: Displaying the Distribution of I/O Throughput for Block Devices \(diskact.d\) 

In the following example, the `diskact.d` script uses `io` provider probes that are enabled by the `sdt` kernel module to display the distribution of I/O throughput for the block devices on the system.

```
#pragma D option quiet

/* diskact.d -- Display the distribution of I/O throughput for block devices */

io:::start
{
  start[args[0]->b_edev, args[0]->b_blkno] = timestamp;
}

io:::done
/start[args[0]->b_edev, args[0]->b_blkno]/
{
  /*
     You want to get an idea of our throughput to this device in KB/sec
     but you have values that are measured in bytes and nanoseconds.
     You want to calculate the following:
    
     bytes / 1024
     ------------------------
     nanoseconds / 1000000000
    
     As DTrace uses integer arithmetic and the denominator is usually
     between 0 and 1 for most I/O, the calculation as shown will lose
     precision. So, restate the fraction as:
    
     bytes         1000000000      bytes * 976562
     ----------- * ------------- = --------------
     nanoseconds   1024            nanoseconds
    
     This is easy to calculate using integer arithmetic.
   */
  this->elapsed = timestamp - start[args[0]->b_edev, args[0]->b_blkno];
  @[args[1]->dev_statname, args[1]->dev_pathname] =
    quantize((args[0]->b_bcount * 976562) / this->elapsed);
  start[args[0]->b_edev, args[0]->b_blkno] = 0;
}

END
{
  printa(" %s (%s)\n%@d\n", @);
}
```

The `#pragma D option quiet` statement is used to suppress unwanted output and the `printa()` function is used to display the results of the aggregation.

See [DTrace Functions](https://docs.oracle.com/en/operating-systems/oracle-linux/dtrace-v2-guide/dtrace_functions.html) for a description of the `printa()` function.

After running the program for approximately a minute, type `Ctrl-C` to display the results:

```
# `dtrace -s diskact.d`
                     `^C`

xvda2 (<unknown>)

            value  ------------- Distribution ------------- count
               -1 | 0
                0 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 3
                1 | 0

xvdc (<unknown>)

            value  ------------- Distribution ------------- count
               -1 | 0
                0 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 3
                1 | 0

xvdc1 (<unknown>)

            value  ------------- Distribution ------------- count
               -1 | 0
                0 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 3
                1 | 0

  dm-0 (<unknown>)

            value  ------------- Distribution ------------- count
              256 | 0
              512 |@@ 1
             1024 |@@ 1
             2048 |@@@@@@ 3
             4096 |@@@@@@@@@@ 5
             8192 |@@@@@@@@@@@@@@@@@ 9
            16384 |@@@@ 2
            32768 | 0
```

### Exercise: Displaying Read and Write I/O Throughput Separately

Create a version of `diskact.d` that aggregates the results separately for reading from, and writing to, block devices. Use a `tick` probe to collect data for 10 seconds.

-   In the actions for `io:::start` and `io:::done`, assign the value of `args[0]->b_flags & B_READ ? "READ" : "WRITE"` to the variable `iodir`.

-   In the actions for `io:::start` and `io:::done`, add `iodir` as a key to the `start[]` associative array.

-   In the action for `io:::done`, add `iodir` as a key to the anonymous aggregation variable `@[]`.

-   Modify the format string for `printa()` to display the value of the `iodir` key.


\(Estimated completion time: 20 minutes\)

### Solution to Exercise: Displaying Read and Write I/O Throughput Separately 

The following example shows a modified version of the `diskact.d` script, which displays separate results for read and write I/O:

```
#pragma D option quiet

/* rwdiskact.d -- Modified version of diskact.d that displays
                  separate results for read and write I/O     */

profile:::tick-10sec
{
  exit(0);
}

io:::start
{
  iodir = args[0]->b_flags & B_READ ? "READ" : "WRITE";
  start[args[0]->b_edev, args[0]->b_blkno, iodir] = timestamp;
}

io:::done
{
  iodir = args[0]->b_flags & B_READ ? "READ" : "WRITE";
  this->elapsed = timestamp - start[args[0]->b_edev,args[0]->b_blkno,iodir];
  @[args[1]->dev_statname, args[1]->dev_pathname, iodir] = 
    quantize((args[0]->b_bcount * 976562) / this->elapsed);
  start[args[0]->b_edev, args[0]->b_blkno,iodir] = 0;}

END
{
  printa(" %s (%s) %s \n%@d\n", @);
}
```

In the example, adding the `iodir` variable to the tuple in the aggregation variable enables DTrace to display separate aggregations for read and write I/O operations.

```
# `dtrace -s rwdiskact.d`
                     `
^C`
  xvda2 (<unknown>) WRITE

            value  ------------- Distribution ------------- count
               -1 | 0
                0 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 1
                1 | 0

  xvdc (<unknown>) WRITE

            value  ------------- Distribution ------------- count
               -1 | 0
                0 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 1
                1 | 0

  xvdc1 (<unknown>) WRITE

            value  ------------- Distribution ------------- count
               -1 | 0
                0 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 1
                1 | 0

  nfs (<nfs>) READ

            value  ------------- Distribution ------------- count
               -1 | 0
                0 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 5
                1 | 0

  dm-0 (<unknown>) WRITE

            value  ------------- Distribution ------------- count
             4096 | 0
             8192 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 1
            16384 | 0
```

### Example: Displaying Cumulative Read and Write Activity Across a File System Device \(fsact\)

The following example is a `bash` shell script that uses an embedded D program to display cumulative read and write block counts for a local file system according to their location on the file system's underlying block device. The `lquantize()` aggregation function is used to display the results linearly as tenths of the total number of blocks on the device.

```
#!/bin/bash

# fsact -- Display cumulative read and write activity across a file system device
#
#          Usage: fsact [<filesystem>]

# Could load the required DTrace modules, if they were not autoloaded.
# grep profile /proc/modules > /dev/null 2>&1 || modprobe profile
# grep sdt /proc/modules > /dev/null 2>&1 || modprobe sdt

# If no file system is specified, assume /
[ $# -eq 1 ] && FSNAME=$1 || FSNAME="/"
[ ! -e $FSNAME ] && echo "$FSNAME not found" && exit 1

# Determine the mountpoint, major and minor numbers, and file system size
MNTPNT=$(df $FSNAME | gawk '{ getline; print $1; exit }')
MAJOR=$(printf "%d\n" 0x$(stat -Lc "%t" $MNTPNT))
MINOR=$(printf "%d\n" 0x$(stat -Lc "%T" $MNTPNT))
FSSIZE=$(stat -fc "%b" $FSNAME)

# Run the embedded D program
dtrace -qs /dev/stdin << EOF
io:::done
/args[1]->dev_major == $MAJOR && args[1]->dev_minor == $MINOR/
{
  iodir = args[0]->b_flags & B_READ ? "READ" : "WRITE";
  /* Normalize the block number as an integer in the range 0 to 10 */ 
  blkno = (args[0]->b_blkno)*10/$FSSIZE;
  /* Aggregate blkno linearly over the range 0 to 10 in steps of 1 */ 
  @a[iodir] = lquantize(blkno,0,10,1)
}

tick-10s
{
  printf("%Y\n",walltimestamp);
  /* Display the results of the aggregation */
  printa("%s\n%@d\n",@a);
  /* To reset the aggregation every tick, uncomment the following line */
  /* clear(@a); */
}
EOF
```

You embed the D program in a shell script so that you can set up the parameters that are needed, which are the major and minor numbers of the underlying device and the total size of the file system in file system blocks. You then access these parameters directly in the D code.

**Note:**

An alternate way of passing values into the D program is to use C preprocessor directives, for example:

```
dtrace -C -D MAJ=$MAJOR -D MIN=$MINOR -D FSZ=$FSSIZE -qs /dev/stdin << EOF
```

You can then refer to the variables in the D program by their macro names instead of their shell names:

```
/args[1]->dev_major == MAJ && args[1]->dev_minor == MIN/

blkno = (args[0]->b_blkno)*10/FSZ;
```

The following example shows output from running the `fsact` command after making the script executable, then running `cp -R` on a directory and `rm -rf` on the copied directory:

```
# `chmod +x fsact`
# `./fsact`
2018 Feb 16 16:59:46
READ

            value  ------------- Distribution ------------- count
              < 0 | 0
                0 |@@@@@@@ 8
                1 |@@@@@@@@@@@@@@@@@@@@@@@@@@@ 32
                2 | 0
                3 | 0
                4 | 0
                5 | 0
                6 | 0
                7 | 0
                8 | 0
                9 | 0
            >= 10 |@@@@@@@ 8

WRITE

            value  ------------- Distribution ------------- count
                9 | 0
            >= 10 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 42                                       0        

`^C`
```

## Displaying System Call Errors

The following information pertains to using the D program `errno.d` to display system call errors.

### Example: Displaying System Call Errors \(`errno.d`\)

The following is an example of the D program, `errno.d`. In this example, the program displays the value of `errno` and the file name if an error occurs when using the `open()` system call to open a file.

```
#!/usr/sbin/dtrace -qs

/* errno.d -- Display errno and the file name for failed open() calls */

syscall::open:entry
{
  self->filename = copyinstr(arg0);
}

syscall::open:return
/arg0 < 0/
{
  printf("errno = %-2d   file = %s\n", errno, self->filename);
}
```

If an error occurs in the `open()` system call, the `return` probe sets the `arg0` argument to `-1` and the value of the built-in `errno` variable indicates the nature of the error. A predicate is used to test the value of `arg0`. Alternatively, you could test whether the value of `errno` is greater than zero.

When you have saved this script to a file and made the file executable, you can then run it to display information about any failures of the `open()` system call that occur on the system. After you have started the script, in a separate terminal window, you can run commands that result in an error, such as running the `ls` command to list a file that does not exist. Or, as in the following example, from another terminal the `cat` command has been issued on a directory, which results in an error:

```
# `./errno.d`

errno = 2    file = /usr/share/locale/en_US.UTF-8/LC_MESSAGES/libc.mo
errno = 2    file = /usr/share/locale/en_US.utf8/LC_MESSAGES/libc.mo
errno = 2    file = /usr/share/locale/en_US/LC_MESSAGES/libc.mo
errno = 2    file = /usr/share/locale/en.UTF-8/LC_MESSAGES/libc.mo
errno = 2    file = /usr/share/locale/en.utf8/LC_MESSAGES/libc.mo
errno = 2    file = /usr/share/locale/en/LC_MESSAGES/libc.mo
`^C`
```

### Exercise: Displaying More Information About System Call Errors

Adapt `errno.d` to display the name of the error instead of its number for any failed system call.

-   The numeric values of errors such as `EACCES` and `EEXIST` are defined in `/usr/include/asm-generic/errno-base.h` and `/usr/include/asm-generic/errno.h`. DTrace defines inline names \(which are effectively constants\) for the numeric error values in `/usr/lib64/dtrace/` *kernel-version* `/errno.d`. Use an associative array named `error[]` to store the mapping between the inline names and the error names that are defined in `/usr/include/asm-generic/errno-base.h`.

-   Use `printf()` to display the user ID, the process ID, the program name, the error name, and the name of the system call.

-   Use the `BEGIN` probe to print column headings.

-   Use the value of `errno` rather than `arg0` to test whether an error from the range of mapped names has occurred in a system call.


\(Estimated completion time: 30 minutes\)

### Solution to Exercise: Displaying More Information About System Call Errors 

The following is an example that shows a modified version of `errno.d`, which displays error names.

#### Example: Modified Version of errno.d Displaying Error Names \(displayerrno.d\) 

```
#!/usr/sbin/dtrace -qs

/* displayerrno.d -- Modified version of errno.d that displays error names */

BEGIN
{
  printf("%-4s %-6s %-10s %-10s %s\n", "UID", "PID", "Prog", "Error", "Func");

  /* Assign error names to the associative array error[] */
  error[EPERM]   = "EPERM";    /* Operation not permitted */
  error[ENOENT]  = "ENOENT";   /* No such file or directory */
  error[ESRCH]   = "ESRCH";    /* No such process */
  error[EINTR]   = "EINTR";    /* Interrupted system call */
  error[EIO]     = "EIO";      /* I/O error */
  error[ENXIO]   = "ENXIO";    /* No such device or address */
  error[E2BIG]   = "E2BIG";    /* Argument list too long */
  error[ENOEXEC] = "ENOEXEC";  /* Exec format error */
  error[EBADF]   = "EBADF";    /* Bad file number */
  error[ECHILD]  = "ECHILD";   /* No child processes */
  error[EAGAIN]  = "EAGAIN";   /* Try again or operation would block */
  error[ENOMEM]  = "ENOMEM";   /* Out of memory */
  error[EACCES]  = "EACCES";   /* Permission denied */
  error[EFAULT]  = "EFAULT";   /* Bad address */
  error[ENOTBLK] = "ENOTBLK";  /* Block device required */
  error[EBUSY]   = "EBUSY";    /* Device or resource busy */
  error[EEXIST]  = "EEXIST";   /* File exists */
  error[EXDEV]   = "EXDEV";    /* Cross-device link */
  error[ENODEV]  = "ENODEV";   /* No such device */
  error[ENOTDIR] = "ENOTDIR";  /* Not a directory */
  error[EISDIR]  = "EISDIR";   /* Is a directory */
  error[EINVAL]  = "EINVAL";   /* Invalid argument */
  error[ENFILE]  = "ENFILE";   /* File table overflow */
  error[EMFILE]  = "EMFILE";   /* Too many open files */
  error[ENOTTY]  = "ENOTTY";   /* Not a typewriter */
  error[ETXTBSY] = "ETXTBSY";  /* Text file busy */
  error[EFBIG]   = "EFBIG";    /* File too large */
  error[ENOSPC]  = "ENOSPC";   /* No space left on device */
  error[ESPIPE]  = "ESPIPE";   /* Illegal seek */
  error[EROFS]   = "EROFS";    /* Read-only file system */
  error[EMLINK]  = "EMLINK";   /* Too many links */
  error[EPIPE]   = "EPIPE";    /* Broken pipe */
  error[EDOM]    = "EDOM";     /* Math argument out of domain of func */
  error[ERANGE]  = "ERANGE";   /* Math result not representable */
}

/* Specify any syscall return probe and test that the value of errno is in range */

syscall:::return
/errno > 0 && errno <= ERANGE/
{
  printf("%-4d %-6d %-10s %-10s %s()\n", uid, pid, execname, error[errno], probefunc);
}
```

```
# `chmod +x displayerrno.d`
# `./displayerrno.d`
UID  PID    Prog       Error      Func
500  3575   test       EACCES     open()
500  3575   test       EINTR      clock_gettime()
`^C`
```

You could modify this program so that it displays verbose information about the nature of the error, in addition to the name of the error.

