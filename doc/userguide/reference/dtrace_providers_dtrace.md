
# DTrace Provider <a id="dt_ref_dt_prov">

The `dtrace` provider includes several probes that are specific to DTrace itself.

Use these probes to initialize state before tracing begins, process state after tracing has completed, and to handle unexpected execution errors in other probes.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## BEGIN Probe <a id="dt_ref_begin_prov">

The `BEGIN` probe fires before any other probe.

No other probe fires until all `BEGIN` clauses have completed. This probe can be used to initialize any state that's needed in other probes. The following example shows how to use the `BEGIN` probe to initialize an associative array to map between `mmap()` protection bits and a textual representation:

```
dtrace:::BEGIN
{
  prot[0] = "---";
  prot[1] = "r--";
  prot[2] = "-w-";
  prot[3] = "rw-";
  prot[4] = "--x";
  prot[5] = "r-x";
  prot[6] = "-wx";
  prot[7] = "rwx";
}

syscall::mmap:entry
{
  printf("mmap with prot = %s", prot[arg2 & 0x7]);
}
```

The `BEGIN` probe fires in an unspecified context, which means the output of `stack` or `ustack`, and the value of context-specific variables such as `execname`, are all arbitrary. These values should not be relied upon or interpreted to infer any meaningful information. No arguments are defined for the `BEGIN` probe.

## END Probe <a id="dt_ref_end_prov">

The `END` probe fires after all other probes.

This probe doesn't fire until all other probe clauses have completed. This probe can be used to process state that has been gathered or to format the output. The `printa` function is therefore often used in the `END` probe. The `BEGIN` and `END` probes can be used together to measure the total time that's spent tracing, for example:

```
dtrace:::BEGIN
{
  start = timestamp;
}

/*
 * ... other tracing functions...
 */

dtrace:::END
{
  printf("total time: %d secs", (timestamp - start) / 1000000000);
}
```

As with the `BEGIN` probe, no arguments are defined for the `END` probe. The context in which the `END` probe fires is arbitrary and can't be depended upon.



**Note:**

The [`exit`](function_exit.md) function causes tracing to stop and the `END` probe to fire.
However, a delay exists between the invocation of the `exit` function and when the `END` probe fires.
During this delay, no further probes can fire.
After a probe invokes the `exit` function, the `END` probe isn't fired until DTrace determines that `exit` has been called and stops tracing.
The rate at which the exit status is checked can be set by using `statusrate` option.

## ERROR Probe <a id="dt_ref_error_prov">

The `ERROR` probe fires when a runtime error occurs during the processing of a clause for a DTrace probe.

When a runtime error occurs, DTrace doesn't process the rest of the clause that resulted in the error. If an ERROR probe is included in the script, it's triggered immediately. After the ERROR probe is processed, tracing continues. If you want a D runtime error to stop all further tracing, you must include an `exit()` action in the clause for the ERROR probe.

In the following example, a clause attempts to dereference a `NULL` pointer and causes the `ERROR` probe to fire. Save it in a file named `error.d`:

```
dtrace:::BEGIN
{
  *(char *)NULL;
}

dtrace:::ERROR
{
  printf("Hit an error!");
}
```

When you run this program, output similar to the following is displayed:

```
dtrace: script 'error.d' matched 2 probes
dtrace: error on enabled probe ID 3 (ID 1: dtrace:::BEGIN): invalid address (0x0) in action #1 at BPF pc 142
CPU     ID                    FUNCTION:NAME
  0      3                           :ERROR Hit an error!

```

The output indicates that the `ERROR` probe fired and that `dtrace` reported the error. `dtrace` has its own enabling of the `ERROR` probe so that it can report errors. Using the `ERROR` probe, you can create custom error handling.

The arguments to the `ERROR` probe are described in the following table.

<table><thead><tr><th>

Argument

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`arg1`

</td><td>

The enabled probe identifier \(EPID\) of the probe that caused the error.

</td></tr><tr><td>

`arg2`

</td><td>

The index of the action that caused the fault.

</td></tr><tr><td>

`arg3`

</td><td>

The DIF offset into the action or -1 if not applicable.

</td></tr><tr><td>

`arg4`

</td><td>

The fault type.

</td></tr><tr><td>

`arg5`

</td><td>

Value that's particular to the fault type.

</td></tr><tbody></table>
The following table describes the various fault types that can be specified in `arg4` and the values that `arg5` can take for each fault type.

<table><thead><tr><th>

`arg4` Value

</th><th>

Description

</th><th>

`arg5` Meaning

</th></tr></thead><tbody><tr><td>

`DTRACEFLT_UNKNOWN`

</td><td>

Unknown fault type

</td><td>

None

</td></tr><tr><td>

`DTRACEFLT_BADADDR`

</td><td>

Access to unmapped or invalid address

</td><td>

Address accessed

</td></tr><tr><td>

`DTRACEFLT_BADALIGN`

</td><td>

Unaligned memory access

</td><td>

Address accessed

</td></tr><tr><td>

`DTRACEFLT_ILLOP`

</td><td>

Illegal or invalid operation

</td><td>

None

</td></tr><tr><td>

`DTRACEFLT_DIVZERO`

</td><td>

Integer divide by zero

</td><td>

None

</td></tr><tr><td>

`DTRACEFLT_NOSCRATCH`

</td><td>

Insufficient scratch memory to satisfy scratch allocation

</td><td>

None

</td></tr><tr><td>

`DTRACEFLT_KPRIV`

</td><td>

Attempt to access a kernel address or property without sufficient privileges

</td><td>

Address accessed or 0 if not applicable

</td></tr><tr><td>

`DTRACEFLT_UPRIV`

</td><td>

Attempt to access a user address or property without sufficient privileges

</td><td>

Address accessed or 0 if not applicable

</td></tr><tr><td>

`DTRACEFLT_TUPOFLOW`

</td><td>

DTrace internal parameter tuple stack overflow

</td><td>

None

</td></tr><tr><td>

`DTRACEFLT_BADSTACK`

</td><td>

Invalid user process stack

</td><td>

Address of invalid stack pointer

</td></tr><tr><td>

`DTRACEFLT_BADSIZE`
</td><td>

Invalid size fault that appears when an invalid size is passed to a function such as `alloca()`, `bcopy()` or `copyin()`.
</td><td>

The invalid size.
</td></tr><tr><td>

`DTRACEFLT_BADINDEX`
</td><td>

Index out of bounds in a scalar array.
</td><td>

The index that was specified.
</td></tr><tr><td>

`DTRACEFLT_LIBRARY`
</td><td>

Library level fault
</td><td>

None.
</td></tr><tbody></table>
If the actions that are taken in the `ERROR` probe cause an error, that error is silently dropped. The `ERROR` probe isn't recursively invoked.

## dtrace Stability <a id="dt_ref_dtstability_prov">

The `dtrace` provider uses DTrace's stability mechanism to describe its stabilities. These values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Stable         | Stable         | Common           |
| Module    | Private        | Private        | Unknown          |
| Function  | Private        | Private        | Unknown          |
| Name      | Stable         | Stable         | Common           |
| Arguments | Stable         | Stable         | Common           |
