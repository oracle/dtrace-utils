
# DTrace Built-in Variable Reference {#dt_ref_builtin_vars}

DTrace includes a set of built-in scalar variables that can be used in D programs or scripts.

## Macro Variables {#dt_macrov_scrpt}

Macro variables are variables that are populated at runtime and identify information about the running `dtrace` process or the process running the compiler.

The D compiler defines a set of built-in macro variables that you can use when writing D programs or interpreter files. Macro variables are identifiers that are prefixed with a dollar sign \(`$`\) and are expanded once by the D compiler when processing an input file or script. The following table describes the macro variables that the D compiler provides.

<table><thead><tr><th>

Name

</th><th>

Description

</th><th>

Reference

</th></tr></thead><tbody><tr><td>

`$[0-9]+`

</td><td>

Macro arguments

</td><td>

See [Macro Arguments](dtrace_builtin_variable_reference.md#macro_arguments)

</td></tr><tr><td>

`$egid`

</td><td>

Effective group ID

</td><td>

See the `getegid(2)` manual page.

</td></tr><tr><td>

`$euid`

</td><td>

Effective user ID

</td><td>

See the `geteuid(2)` manual page.

</td></tr><tr><td>

`$gid`

</td><td>

Real group ID

</td><td>

See the `getgid(2)` manual page.

</td></tr><tr><td>

`$pid`

</td><td>

Process ID

</td><td>

See the `getpid(2)` manual page.

</td></tr><tr><td>

`$pgid`

</td><td>

Process group ID

</td><td>

See the `getpgid(2)` manual page.

</td></tr><tr><td>

`$ppid`

</td><td>

Parent process ID

</td><td>

See the `getppid(2)` manual page.

</td></tr><tr><td>

`$sid`

</td><td>

Session ID

</td><td>

See the `getsid(2)` manual page.

</td></tr><tr><td>

`$target`

</td><td>

Target process ID

</td><td>

See [Target Process ID](dtrace_builtin_variable_reference.md#target_process)
</td></tr><tr><td>

`$uid`

</td><td>

Real user ID

</td><td>

See the `getuid(2)` manual page

</td></tr><tbody></table>
The variables expand to the attribute value associated with the current `dtrace` process or whatever process is running the D compiler. All the macro variables expand to integers that correspond to system attributes, such as the process ID and the user ID, except the `$[0-9]+` macro arguments and the `$target` macro variable.

Using macro variables in interpreter files lets you create persistent D programs that you don't need to edit every time you want to use them. For example, to count all system calls, except those that are run by the `dtrace` command, use the following D program clause containing `$pid`:

```
syscall:::entry
/pid != $pid/
{
  @calls = count();
}
```

This clause always behaves as expected, even though each invocation of the `dtrace` command has a different process ID. Macro variables can be used in a D program anywhere that an integer, identifier, or string can be used.

Macro variables are expanded only one time when the input file or script is parsed, not recursively.

Except in probe descriptions, each macro variable is expanded to form a separate input token and can't be concatenated with other text to yield a single token.

For example, if `$pid` expands to the value `456`, the D code in the following example would expand to the two adjacent tokens `123` and `456`, resulting in a syntax error, rather than the single integer token `123456`:

```
123$pid
```

However, in probe descriptions, macro variables are expanded and concatenated with adjacent text.

Macro variables are only expanded one time within each probe description field and they can't contain probe description delimiters \(`:`\).

### Macro Arguments {#dt_macrov_scrpt}

The D compiler also provides a set of macro variables corresponding to any more argument operands that are specified as part of the `dtrace` command invocation. These *macro arguments* are accessed by using the built-in names `$0`, for the name of the D program file or `dtrace` command, `$1`, for the first extra operand, `$2` for the second operand, and so on. If you use the `-s` option, `$0` expands to the value of the name of the input file that's used with this option. For D programs that are specified on the command line, `$0` expands to the value of `argv[0]`, which is used to run the `dtrace` command itself.

Macro arguments can expand to integers, identifiers, or strings, depending on the form of the corresponding text. As with all macro variables, macro arguments can be used anywhere integer, identifier, and string tokens can be used in a D program.

All the following examples could form valid D expressions assuming appropriate macro argument values:

```
execname == $1  /* with a string macro argument */

x += $1         /* with an integer macro argument */

trace(x->$1)    /* with an identifier macro argument */
```

Macro arguments can be used to create DTrace interpreter files that run as normal Linux commands and use information that's specified by a user or by another tool to change their behavior.

For example, the following D interpreter file traces `write()` system calls that are run by a particular process ID and saved in a file named `tracewrite`:

```
#!/usr/sbin/dtrace -s 
syscall::write:entry
/pid == $1/
{
}
```

If you make this interpreter file executable, you can specify the value of `$1` by using an extra command line argument after the interpreter file, for example:

```
sudo chmod a+rx ./tracewrite
sudo ./tracewrite 12345
```

The resulting command invocation counts each `write()` system call that's made by the process ID 12345.

If a D program references a macro argument that isn't provided on the command line, an appropriate error message is printed and the program fails to compile, as shown in the following example output:

```
dtrace: failed to compile script ./tracewrite: line 4: 
  macro argument $1 is not defined
```

D programs can reference unspecified macro arguments if you set the `defaultargs` option. If `defaultargs` is set, unspecified arguments have the value `0`. See [DTrace Runtime and Compile-time Options Reference](dtrace_runtime_options.md) for more information about D compiler options. The D compiler also produces an error message if other arguments that aren't referenced by the D program are specified on the command line.

The macro argument values must match the form of an integer, identifier, or string. If the argument doesn't match any of these forms, the D compiler reports an appropriate error message. When specifying string macro arguments to a DTrace interpreter file, surround the argument in an extra pair of single quotes to avoid interpretation of the double quotes and string contents by the shell:

```
sudo ./foo '"a string argument"'
```

If you want D macro arguments to be interpreted as string tokens, even if they match the form of an integer or identifier, prefix the macro variable or argument name with two leading dollar signs, for example, `$$1`, which forces the D compiler to interpret the argument value as if it were a string surrounded by double quotes. All the usual D string escape sequences, per [Table 5](dtrace-ref-TypesOperatorsandExpressions.md#dt_t6_dlang), are expanded inside any string macro arguments, regardless of whether they're referenced by using the `$arg` or `$$arg` form of the macro. If the `defaultargs` option is set, unspecified arguments that are referenced with the `$$arg` form have the value of the empty string \(`""`\).

### Target Process ID

Use the `$target` macro variable to create scripts to be applied to the user process of interest that you specify with the `-p` option or that you create by using the `dtrace` command with the `-c` option. The D programs that you specify on the command line or by using the `-s` option are compiled after processes are created or grabbed, and the `$target` variable expands to the integer process ID of the first such process.

For example, you could use the following D script to find the distribution of system calls that are made by a particular subject process. Save it in a file named `syscall.d`:

```
syscall:::entry
/pid == $target/
{
  @[probefunc] = count();
}
```

To find the number of system calls made by the `date` command, save the script in the file named `syscall.d`, then run the following command:

```
sudo dtrace -s syscall.d -c date
```

## args\[\] {#dt_ref_vars_args}

The typed and mapped arguments, if any, to the current probe. The `args[]` array is accessed using an integer index. Use `dtrace -l -v` and check `Argument Types` for the type of each argument of each probe. For example, consider the system call `prlimit()`. The prototype on its `man` page \(`man -s 2 prlimit`\) is consistent with its DTrace probe listing \(`dtrace -lvn 'syscall:vmlinux:prlimit*:entry' | grep args`\). Specifically, argument 2, if non NULL, points to a `struct rlimit` with the requested resource limit, which can be traced with:

```
syscall:vmlinux:prlimit*:entry
/args[2] != NULL/
{
    printf("request limit %d for resource %d\n", args[2]->rlim_cur, args[1]);
}
```

## arg0, …, arg9 {#dt_ref_var_arg0-9}

```
int64_t arg0, ..., arg9
```

The built-in variables `arg0`, `arg1` and so on, are the first ten input arguments to a probe, untyped and unmapped, represented as 64-bit integers. Values are meaningful only for arguments defined for the current probe. For example, the command `dtrace -lvn 'rawfbt:vmlinux:ksys_write:entry` indicates that the probe has no typed arguments. Yet we know that kernel function `ksys_write()` has an `arg1` that points to a buffer that's to be written. It might be accessed using:

```
rawfbt:vmlinux:ksys_write:entry
/pid == $target/
{
    printf("%s\n", stringof(arg1));
}
```

## caller {#dt_ref_var_caller}

```
uintptr_t caller
```

The built-in variable `caller` references the program counter location of the current kernel thread at the time the probe fired.

## curcpu {#dt_ref_var_curcpu}

```
cpuinfo_t * curcpu
```

The built-in variable `curcpu` references the current physical CPU.

## curthread {#dt_ref_var_curthread}

```
vmlinux`struct task_struct * curthread
```

The built-in variable `curthread` references a `vmlinux` data type, for which members can be found by searching for "task\_struct" on the Internet.

## epid {#dt_ref_var_epid}

```
uint_t epid
```

The built-in variable `epid` references the enabled probe ID \(EPID\) for the current probe. This integer uniquely identifies a particular probe that's enabled with a specific predicate and set of functions.

## errno

```
int errno {#dt_ref_var_errno}
```

The built-in variable `errno` references the error value returned by the last system call run by this thread.

## execname {#dt_ref_var_execname}

```
string execname
```

The built-in variable `execname` references the name that was passed to `execve()` to run the current process.

## fds {#dt_ref_var_fds}

```
fileinfo_t fds[]
```

The built-in `variable fds[]` is an array which has the files the current process has opened in a `fileinfo_t` array, indexed by file descriptor number. See [fileinfo\_t](dtrace_providers_io.md#).

## gid {#dt_ref_var_gid}

```
gid_t gid
```

The built-in variable `gid` references the real group ID of the current process.

## id {#dt_ref_var_id}

```
uint_t id
```

The built-in variable `id` references the probe ID for the current probe. This ID is the system-wide unique identifier for the probe, as published by DTrace and listed in the output of `dtrace -l`.

## ipl {#dt_ref_var_ipl}

```
uint_t ipl
```

The built-in variable `ipl` references the interrupt priority level \(IPL\) on the current CPU at probe firing time.

**Note:**

This value is non-zero if interrupts are firing and zero otherwise. The non-zero value depends on whether preemption is active, and other factors, and can vary between kernel releases and kernel configurations.

## pid {#dt_ref_var_pid}

```
pid_t pid
```

The built-in variable `pid` references the process ID of the current process.

## ppid {#dt_ref_var_ppid}

```
pid_t ppid
```

The built-in variable `ppid` references the parent process ID of the current process.

## probefunc {#dt_ref_var_probefunc}

```
string probefunc
```

The built-in variable `probefunc` references the function name part of the current probe's description.

## probemod {#dt_ref_var_probemod}

```
string probemod
```

The built-in variable `probemod` references the module name part of the current probe's description.

## probename {#dt_ref_var_probename}

```
string probename
```

The built-in variable `probename` references the name part of the current probe's description.

## probeprov {#dt_ref_var_probeprov}

```
string probeprov
```

The built-in variable `probeprov` references the provider name part of the current probe's description.

## stackdepth {#dt_ref_var_stackdepth}

```
uint32_t stackdepth
```

The built-in variable `stackdepth` references the current thread's stack frame depth at probe firing time.

## tid {#dt_ref_var_tid}

```
id_t tid
```

The built-in variable `tid` references the thread ID of the current thread.

## timestamp {#dt_ref_var_timestamp}

```
uint64_t timestamp
```

The built-in variable `timestamp` references the current value of a nanosecond timestamp counter. This counter increments from an arbitrary point in the past. Therefore, only use the timestamp counter for relative computations.

## ucaller {#dt_ref_var_ucaller}

```
uint64_t ucaller
```

The built-in variable `ucaller` references the program counter location of the current user thread at the time the probe fired.

## uid {#dt_ref_var_uid}

```
uid_t uid
```

The built-in variable `uid` references the real user ID of the current process.

## uregs {#dt_ref_var_uregs}

```
uint64_t uregs[]
```

The current thread's saved user-mode register values at probe firing time.

## ustackdepth {#dt_ref_var_ustackdepth}

```
uint32_t ustackdepth
```

The built-in variable `ustackdepth` references the user thread's stack frame depth at probe firing time.

## vtimestamp {#dt_ref_var_vtimestamp}

```
uint64_t vtimestamp
```

The built-in variable `vtimestamp` references the current value of a nanosecond timestamp counter that's virtualized to the amount of time that the current thread has been running on a CPU, minus the time spent in DTrace predicates and functions. This counter increments from an arbitrary point in the past. Therefore, only use the vtimestamp counter for relative time computations.

## walltimestamp {#dt_ref_var_walltimestamp}

```
int64_t walltimestamp
```

The built-in variable `walltimestamp` references the current number of nanoseconds since 00:00 Universal Coordinated Time, January 1, 1970.

