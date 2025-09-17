
# About the dtrace Command {#dtrace_command_desc}

The `dtrace` command provides a generic interface to all the essential services that are provided by the DTrace facility.

The `dtrace` command includes options to do the following:

-   List the set of probes and providers published by DTrace.

-   Enable probes directly by using any of the probe description specifiers \(provider, module, function, name\).

-   Run the D compiler and compile one or more D program files or programs written directly on the command line.

-   Generate program stability reports.

-   Change DTrace tracing and buffering behavior and enable extra D compiler features.


You can also use the `dtrace` command to create D scripts by using the command in a `#!` declaration to create an interpreter file. Finally, you can use the `-e` option to `dtrace` to compile D programs and find their properties without enabling any tracing.

**Parent topic:**[DTrace Command Reference](../reference/dtrace_command_reference.md)

