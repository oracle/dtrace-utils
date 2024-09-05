<!--
SPDX-FileCopyrightText: 2013,2024 Oracle and/or its affiliates.
SPDX-License-Identifier: CC-BY-SA-4.0
-->

---
publisherinformation: September 2022
---

# Introducing DTrace

This chapter introduces the dynamic tracing \(DTrace\) facility of Oracle Linux. You can use DTrace to examine the behavior of the operating system and of user-space programs that have been instrumented with DTrace probes. The Unbreakable Enterprise Kernel \(UEK\) build is enabled to run DTrace. UEK is the kernel that is compiled for use on Oracle Linux. You could run a Linux kernel other than UEK on Oracle Linux, but DTrace would most likely not be enabled and available, as UEK is the kernel that is included with Oracle Linux.

Note that more recent versions of UEK often have better DTrace functionality than earlier versions. The largest set of DTrace features and improvements are available on the latest DTrace-enabled kernels.

This tutorial assumes that you are using UEK on the x86\_64 architecture. Note that UEK releases for any other architectures might not include support for all of the providers that are discussed in this tutorial.

## About This Tutorial

This tutorial includes a variety of DTrace scripts and describes different ways in which you can use DTrace. Several examples have additional exercises that offer further practice in using DTrace. Each exercise provides an estimate of the time that you should allow to complete it. Depending on your level of programming knowledge, you might need more or less time. You should already have a good understanding of Linux administration and system programming, and broad experience using a programming language, such as C or C++, and a scripting language, such as Python. If you are not familiar with terms such as *system call*, *type*, *cast*, *signal*, *struct*, or *pointer*, you might have difficulty in understanding some of the examples or completing some of the exercises in this tutorial. However, each exercise provides a sample solution in case you do get stuck. You are encouraged to experiment with the examples to develop your skills at creating DTrace scripts.

CAUTION:

To run the examples and perform the exercises in this tutorial, you need to have `root` access to a system. Only the `root` user or a user with `sudo` access to run commands as `root` can use the `dtrace` utility. As `root`, you have total power over a system and so have total responsibility for that system. Note that although DTrace is designed so that you can use it safely without needing to worry about corrupting the operating system or other processes, there are ways to circumvent the built-in safety measures.

To minimize risk, perform the examples and exercises in this tutorial on a system other than a production system.

The examples in this tutorial demonstrate the different ways that you can perform dynamic tracing of your system: by entering a simple D program as an argument to `dtrace` on the command line, by using the `dtrace` command to run a script that contains a D program, or by using an executable D script that contains a *hashbang* \(`#!` or *shebang*\) invocation of `dtrace`. When you create your own D programs, you can choose which method best suits your needs.

## About DTrace

DTrace is a comprehensive dynamic tracing facility that was first developed for use on the Solaris operating system \(now known as Oracle Solaris\) and subsequently ported to Oracle Linux. You can use DTrace to explore the operation of your system to better understand how it works, to track down performance problems across many layers of software, or to locate the causes of aberrant behavior.

Using DTrace, you can record data at previously instrumented places of interest, which are referred to as *probes*, in kernel and user-space programs. A probe is a location to which DTrace can bind a request to perform a set of actions, such as recording a stack trace, a timestamp, or the argument to a function. Probes function like programmable sensors that record information. When a probe is triggered, DTrace gathers data that you have designated in a D script and reports this data back to you.

Using DTrace's D programming language, you can query the system probes to provide immediate and concise answers to any number of questions that you might formulate.

A D program describes the actions that occur if one or more specified probes is triggered. A probe is uniquely specified by the name of the DTrace provider that publishes the probe, the name of the module, library, or user-space program in which the probe is located, the name of the function in which the probe is located, and the name of the probe itself, which usually describes some operation or functionality that you can trace. Because you do not need to specify probes exactly, this allows DTrace to perform the same action for a number of different probes. Full and explicit representation of a single probe in the D language takes the form:

*PROVIDER*:*MODULE*:*FUNCTION*:*NAME*

When you use the `dtrace` command to run a D program, you invoke the compiler for the D language. When DTrace has compiled your D program into a safe, intermediate form, it sends it to the DTrace module in the operating system kernel for execution. The DTrace module activates the probes that your program specifies and executes the associated actions when your probes fire. DTrace handles any runtime errors that might occur during your D program's execution, including dividing by zero, dereferencing invalid memory, and so on, and reports them to you.

**Note:**

**Modules Primer**

There are two kinds of modules that are frequently referenced in most, if not all, detailed discussions of DTrace on Oracle Linux. To avoid confusion, you must identify which kind of module is being discussed with any mention of a *module*. The context usually gives plenty of clues, if you have knowledge of these kinds of modules.

The module that is being discussed in the sequence *PROVIDER*:*MODULE*:*FUNCTION*:*NAME* refers to a module in the sense that it is a distinct, orderly component that is used by DTrace to reference and represent areas of code. You can specify that a DTrace module reference point DTrace to some set of code or functionality. Output from a `dtrace` command uses *MODULE* to convey that some activity has occurred in such an area of code in the kernel or in a user-space program. This type of module can simply be referred to as a *DTrace module*.

A second and very different meaning for the term *module* is a Linux kernel module. The Linux kernel is divided into different functional components that are called modules: these modules might be loaded and unloaded separately from each other. The output of the `lsmod` command shows which Linux kernel modules are loaded on the system. These modules are referred to as *Linux kernel modules*, or within the context of discussing only Linux, simply *kernel modules*.

The following are two additional variations of other module references:

-   Some Linux kernel modules that are specific to DTrace must be present to use DTrace on a Linux system. These particular kernel modules are specifically referenced as *dtrace kernel modules*. See the table in [About DTrace Providers](dtrace-tutorial-IntroducingDTrace.md#) for a list of providers that are available from specific `dtrace` kernel modules.

-   DTrace probes must be compiled into any kernel module in order for DTrace to monitor the activity in the kernel module. However, kernel modules with DTrace probes are not `dtrace` kernel modules, rather, they are referred to as *DTrace enabled kernel modules*. All kernel modules that can be traced by DTrace implicitly are DTrace enabled kernel modules and therefore are not typically referred to explicitly as DTrace enabled kernel modules, but with the shorthand, *kernel modules*.


Unless you explicitly permit DTrace to perform potentially destructive actions, you cannot construct an unsafe program that would cause DTrace to inadvertently damage either the operating system or any process that is running on your system. These safety features enable you to use DTrace in a production environment without worrying about crashing or corrupting your system. If you make a programming mistake, DTrace reports the error and deactivates your program's probes. You can then correct your program and try again.

For more information about using DTrace, see the [Oracle Linux: Using DTrace for System Tracing](https://docs.oracle.com/en/operating-systems/oracle-linux/dtrace-v2-guide/).

## About DTrace Providers

The following table lists the providers that are included with the Oracle Linux implementation of DTrace and the kernel modules that include the providers.

<table><thead><tr><th>

Provider

</th><th>

`dtrace` Kernel Module

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`dtrace`

</td><td>

`dtrace`

</td><td>

Provides probes that relate to DTrace itself, such as `BEGIN`, `ERROR`, and `END`. You can use these probes to initialize DTrace's state before tracing begins, process its state after tracing has completed, and handle unexpected execution errors in other probes.

</td></tr><tr><td>

`fbt`

</td><td>

`fbt`

</td><td>

Supports function boundary tracing \(FBT\) probes, which are at the entry and exits of kernel functions.

</td></tr><tr><td>

`fasttrap`

</td><td>

`fasttrap`

</td><td>

Supports user-space tracing of DTrace-enabled applications.

</td></tr><tr><td>

`io`

</td><td>

`sdt`

</td><td>

Provides probes that relate to data input and output. The `io` provider enables quick exploration of behavior observed through I/O monitoring.

</td></tr><tr><td>

`IP`

</td><td>

`sdt`

</td><td>

Provides probes for the IP protocol \(both IPv4 and IPv6\).

</td></tr><tr><td>

`lockstat`

</td><td>

`sdt`

</td><td>

Provides probes for locking events including: mutexes, read-write locks, and spinlock.

</td></tr><tr><td>

`perf`

</td><td>

`sdt`

</td><td>

Provides probes that correspond to each `perf` tracepoint, including typed arguments.

</td></tr><tr><td>

`proc`

</td><td>

`sdt`

</td><td>

Provides probes for monitoring process creation and termination, LWP creation and termination, execution of new programs, and signal handling.

</td></tr><tr><td>

`profile`

</td><td>

`profile`

</td><td>

Provides probes that are associated with an asynchronous interrupt event that fires at a fixed and specified time interval, rather than with any particular point of execution. You can use these probes to sample some aspect of a system's state.

</td></tr><tr><td>

`sched`

</td><td>

`sdt`

</td><td>

Provides probes related to CPU scheduling. Because CPUs are the one resource that all threads must consume, the `sched` provider is very useful for understanding systemic behavior.

</td></tr><tr><td>

`syscall`

</td><td>

`systrace`

</td><td>

Provides probes at the entry to and return from every system call. Because system calls are the primary interface between user-level applications and the operating system kernel, these probes can offer you an insight into the interaction between applications and the system.

</td></tr><tr><td>

`TCP`

</td><td>

`sdt`

</td><td>

Provides probes in the code that implements the TCP protocol, for both IPv4 and IPv6.

</td></tr><tr><td>

`UDP`

</td><td>

`sdt`

</td><td>

Provides probes in the code that implements the UDP protocol, for both IPv4 and IPv6.

</td></tr><tbody></table>
SDT is a multi-provider, in that it implements multiple providers under the same provider.

The `fasttrap` provider is considered a meta-provider, which means it is a provider framework. The `fasttrap` meta-provider is used to facilitate the creation of providers that are instantiated for user-space processes.

See [DTrace Provider Reference](https://docs.oracle.com/en/operating-systems/oracle-linux/dtrace-v2-guide/dtrace_providers.html) for more information about providers and their probes.

## Preparing to Install and Configure DTrace

**Note:**

The DTrace package \(`dtrace-utils`\) is available from ULN. To ensure best results, your system must be registered with ULN and should be installed with or updated to the latest Oracle Linux release.

To install and configure DTrace, perform the following steps:

1.  If your system is not already running the latest UEK version:

    1.  Update your system to the latest UEK release:

        ```
        # `yum update`
        ```

    2.  Reboot the system and select the latest UEK version that is available in the boot menu. Typically, this is the default kernel.

2.  Install the DTrace utilities package:

    ```
    # `yum install dtrace-utils`
    ```


### Using Automatically Loaded DTrace Modules

**Note:**

The following is a quick-start method for using DTrace kernel modules that are automatically loaded. If you plan to manually load DTrace kernel modules, see [Manually Loading DTrace Modules](dtrace-tutorial-IntroducingDTrace.md#) for instructions.

DTrace automatically loads some `dtrace` kernel modules when the `dtrace` command references the probes that are associated with a `dtrace` kernel module. You can use this convenient method to load `dtrace` modules, rather than manually loading them.

To find out which modules are automatically loaded in this manner, use the following command:

```
# `cat /etc/dtrace-modules`
sdt
systrace
profile
fasttrap
```

Additional modules can be added to this list after it is determined that they are fully tested.

To determine whether a particular module has been loaded in the Linux kernel, use the `lsmod` command. For example, you would run the following command to determine whether the `sdt` module is loaded:

```
# `lsmod | grep sdt`
```

If the module is not loaded, the command most likely will result in no output. If the module is loaded, the output is similar to the following:

```
sdt                    20480   0
dtrace                 151552  4 sdt,fasttrap,systrace,profile
```

### Manually Loading DTrace Modules

**Note:**

The following information describes how to manually load DTrace kernel modules. If you plan to use DTrace kernel modules that are automatically loaded, you can skip this section of the tutorial. See [Using Automatically Loaded DTrace Modules](dtrace-tutorial-IntroducingDTrace.md#).

To use DTrace providers, their supported kernel modules must be loaded each time the kernel is booted.

If the `dtrace` kernel module is not already loaded, when the `dtrace` command is run, the `dtrace` module and all of the modules that are listed in `/etc/dtrace-modules` are all loaded automatically. However, if the `dtrace` kernel module is already loaded, the automatic kernel module loading mechanism is not triggered.

You can load modules manually by using the `modprobe` command. For example, to use the `fbt` kernel module if it is not in the default list, you would run the following command:

```
# `modprobe fbt`
```

The `modprobe` action also loads the `dtrace` kernel module as a dependency so that a subsequent `dtrace` command does not drive automatic loading of other `dtrace` modules until the `dtrace` kernel module is no longer loaded. The `drace` kernel module will no longer be loaded upon another boot of the system or after the manual removal of the `dtrace` kernel modules.

The suggested practice is to use the `dtrace -l` command to trigger automatic module loading and thereby also confirm basic `dtrace` functionality. Then, use the `modprobe` command to load additional modules that are not in the default list, such as `fbt`, as needed.

#### Example: Displaying Probes for a Provider

The following example shows how you would display the probes for a provider, such as `proc`, by using the `dtrace` command.

```
# `dtrace -l -P proc`
   ID   PROVIDER            MODULE                          FUNCTION NAME
  855       proc           vmlinux                          _do_fork lwp-create
  856       proc           vmlinux                          _do_fork create
  883       proc           vmlinux                           do_exit lwp-exit
  884       proc           vmlinux                           do_exit exit
  931       proc           vmlinux                   do_sigtimedwait signal-clear
  932       proc           vmlinux                     __send_signal signal-send
  933       proc           vmlinux                     __send_signal signal-discard
  941       proc           vmlinux                     send_sigqueue signal-send
  944       proc           vmlinux                        get_signal signal-handle
 1044       proc           vmlinux                     schedule_tail start
 1045       proc           vmlinux                     schedule_tail lwp-start
 1866       proc           vmlinux                do_execveat_common exec-failure
 1868       proc           vmlinux                do_execveat_common exec
 1870       proc           vmlinux                do_execveat_common exec-success
```

The output shows the numeric identifier of the probe, the name of the probe provider, the name of the probe module, the name of the function that contains the probe, and the name of the probe itself.

The full name of a probe is `*PROVIDER*:*MODULE*:*FUNCTION*:*NAME*`, for example, `proc:vmlinux:_do_fork:create`. If no ambiguity exists with other probes for the same provider, you can usually omit the `*MODULE*` and `*FUNCTION*` elements when specifying a probe. For example, you can refer to `proc:vmlinux:_do_fork:create` as `proc::_do_fork:create` or `proc:::create`. If several probes match your specified probe in a D program, the associated actions are performed for each probe.

These probes enable you to monitor how the system creates processes, executes programs, and handles signals.

If you checked previously and the `sdt` module was not loaded, check again to see if the `dtrace` command has loaded the module.

If the following message is displayed after running the `dtrace -l -P proc` command \(instead of output similar to the output in the previous example\), it is an indication that the module has not loaded:

```
No probe matches description
```

If the `sdt` module does not load automatically on a system with DTrace properly installed, it is because another DTrace module was manually loaded by using the `modprobe` command. Manually loading a DTrace module in this way effectively prevents any other modules from being automatically loaded by the `dtrace` command until the system is rebooted. In this instance, one workaround is to use the `modprod` command to manually load the `sdt` module. When the module has successfully loaded, you should see a probe listing similar to the output in [Example: Displaying Probes for a Provider](dtrace-tutorial-IntroducingDTrace.md#) when you re-issue the `dtrace` command.

#### Exercise: Enabling and Listing DTrace Probes

Try listing the probes of the `syscall` provider. Notice that both `entry` and `return` probes are provided for each system call.

\(Estimated completion time: 3 minutes\)

#### Solution to Exercise: Enabling and Listing DTrace Probes

```
# `dtrace -l -P syscall`
  ID   PROVIDER            MODULE                  FUNCTION NAME
   4    syscall           vmlinux                      read entry
   5    syscall           vmlinux                      read return
   6    syscall           vmlinux                     write entry
   7    syscall           vmlinux                     write return
   8    syscall           vmlinux                      open entry
   9    syscall           vmlinux                      open return
  10    syscall           vmlinux                     close entry
  11    syscall           vmlinux                     close return
...
 646    syscall           vmlinux             pkey_mprotect entry
 647    syscall           vmlinux             pkey_mprotect return
 648    syscall           vmlinux                pkey_alloc entry
 649    syscall           vmlinux                pkey_alloc return
 650    syscall           vmlinux                 pkey_free entry
 651    syscall           vmlinux                 pkey_free return
 652    syscall           vmlinux                     statx entry
 653    syscall           vmlinux                     statx return
 654    syscall           vmlinux                    waitfd entry
 655    syscall           vmlinux                    waitfd return
```

**Note:**

The probe IDs numbers might differ from those on your system, depending on what other providers are loaded.

## Running a Simple DTrace Program

The following example shows how you would use a text editor to create a new file called `hello.d` and then type a simple D program.

### Example: Simple D Program That Uses the BEGIN Probe \(hello.d\)

```
/* hello.d -- A simple D program that uses the BEGIN probe */

BEGIN
{
  /* This is a C-style comment */
  trace("hello, world");
  exit(0);
}
```

A D program consists of a series of clauses, where each clause describes one or more probes to enable, and an optional set of actions to perform when the probe fires. The actions are listed as a series of statements enclosed in braces `{}` following the probe name. Each statement ends with a semicolon \(`;`\).

In this example, the function `trace` directs DTrace to record the specified argument, the string ”hello, world”, when the `BEGIN` probe fires, and then print it out. The function `exit()` tells DTrace to cease tracing and exit the `dtrace` command.

The full name of the `BEGIN` probe is `dtrace:::BEGIN`. `dtrace` provides three probes: `dtrace:::BEGIN`, `dtrace:::END`, and `dtrace:::ERROR`. Because these probe names are unique to the `dtrace` provider, their names can be shortened to `BEGIN`, `END`, and `ERROR`.

When you have saved your program, you can run it by using the `dtrace` command with the `-s` option, which specifies the name of the file that contains the D program:

```
# `dtrace -s hello.d`
dtrace: script 'hello.d' matched 1 probe
CPU     ID                    FUNCTION:NAME
  0      1                           :BEGIN   hello, world
```

DTrace interprets and runs the script. You will notice that in addition to the string `"hello,world"`, the default behavior of DTrace is to display information about the CPU on which the script was running when a probe fired, the ID of the probe, the name of the function that contains the probe, and the name of the probe itself. The function name is displayed as blank for `BEGIN`, as DTrace provides this probe.

You can suppress the probe information in a number of different ways, for example, by specifying the `-q` option:

```
# `dtrace -q -s hello.d`
hello, world
```

### Exercise: Using the END Probe

Copy the `hello.d` program to the file `goodbye.d`. Edit this file so that it traces the string "goodbye, world" and uses the `END` probe instead of `BEGIN`. When you run this new script, you need to type `Ctrl-C` to cause the probe to fire and exit `dtrace`.

\(Estimated completion time: 5 minutes\)

### Solution to Exercise and Example: Using the END Probe



The following is an example of a simple D program that demonstrates the use of the `END` probe:

```
/* goodbye.d -- Simple D program that demonstrates the END probe */

END
{
  trace("goodbye, world");
}
```

```
# `dtrace -s goodbye.d`
dtrace: script 'goodbye.d' matched 1 probe
`^C`
CPU     ID                    FUNCTION:NAME
  3      2                             :END   goodbye, world
```

```
# `dtrace -q -s ./goodbye.d`
                     `^C`
goodbye, world
```

