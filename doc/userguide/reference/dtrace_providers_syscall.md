
# Syscall Provider <a id="dt_ref_syscall_prov">

The `syscall` provider makes available a probe at the entry to and return from every system call in the system.

Because system calls are the primary interface between user-level applications and the OS kernel, the `syscall` provider can offer tremendous insight into application behavior about the system.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## syscall Probes <a id="dt_ref_syscallprobes_prov">

`syscall` provides a pair of probes for each system call: an `entry` probe that fires before the system call is entered, and a `return` probe that fires after the system call has completed, but before control has been transferred back to user-level. For all `syscall` probes, the function name is set as the name of the instrumented system call.

Often, the system call names that are provided by `syscall` correspond to names in the Section 2 manual pages. However, some `syscall` provider probes don't directly correspond to any documented system call, such as the case where a system call might be a sub operation of another system call or where a system call might be private in that they span the user-kernel boundary.

## syscall Probe Arguments <a id="dt_ref_syscallargs_prov">

For `entry` probes, the arguments, `arg0` ... `arg*n*`, are arguments to the system call. For return probes, both `arg0` and `arg1` contain the return value. A non-zero value in the D variable `errno` indicates a system call failure.

## syscall Stability <a id="dt_ref_syscallstab_prov">

The `syscall` provider uses DTrace's stability mechanism to describe its stabilities. These stability values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Evolving       | Evolving       | Common           |
| Module    | Private        | Private        | Unknown          |
| Function  | Private        | Private        | ISA              |
| Name      | Evolving       | Evolving       | Common           |
| Arguments | Private        | Private        | ISA              |
