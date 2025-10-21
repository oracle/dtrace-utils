
# DTrace Components and Terminology <a id="concept_terms">

Learn about the different components and the terms used to describe them within the DTrace framework.

DTrace is a framework that dynamically traces data into buffers that are read by the `dtrace` command line utility. The `dtrace` command line utility can run programs that can implement certain functions by compiling D programs to generate eBPF code that's loaded into the kernel. In practice, all interaction with DTrace is performed by using the `dtrace` command line utility. See [Install DTrace](../how-to/dtrace-howto-install-dtrace.md) for information on how to install the command line utility.

**Parent topic:**[DTrace Concepts](../explanation/dtrace-concepts.md)

## Probes <a id="concept_terms_probes">

DTrace works by using *probes* that identify particular instrumentation in the kernel or within a user space application, or which can be used to identify interval counters or performance event counters. Events such as when particular code is run or when a specific counter is incremented cause a probe to fire and DTrace can perform functions that are bound to the event in a program or script. For example, a probe can fire when a particular file is opened and a DTrace program can print information related to the event that can be useful for debugging or resolving an issue. Equally, at the moment that DTrace starts or ends any tracing activity, the `BEGIN` and `END` probes dedicated to these actions always fire.

You can list all the available probes on a system by typing the following command:

```
sudo dtrace -l
```

Output is displayed to show each of the different values that are used to reference a probe correctly:

```
   ID   PROVIDER            MODULE                          FUNCTION NAME
    1     dtrace                                                     BEGIN
    2     dtrace                                                     END
    3     dtrace                                                     ERROR
    4    syscall           vmlinux                              read entry
    5    syscall           vmlinux                              read return
    6    syscall           vmlinux                             write entry
    7    syscall           vmlinux                             write return
    ...
```

See [List and Enable Probes](../how-to/dtrace-howto-list-and-enable-probes.md) for more information on how to list and enable specific probes.

Probes are made available by *providers*, which group particular kinds of instrumentation together. If a provider is related to source code, its probes might also include information about the piece of code that the probe relates to in a *module* and a *function* identifier. Therefore, a probe is identified by a *probe description*, grouped into four fields:

-   **provider**

    The name of the DTrace provider that the probe belongs to.

-   **module**

    If the probe corresponds to a specific program location, the name of the kernel module, library, or user-space program in which the probe is found. Some probes might be associated with a module name that isn't tied to a particular source location in cases where they relate to more abstract tracepoints.

-   **function**

    If the probe corresponds to a specific program location, the name of the program function in which the probe is found.

-   **name**

    The name that provides some idea of the probe's semantic meaning, such as `BEGIN` or `END`.


When referencing a probe, write all four parts of the probe description separated by colons:

```
*provider*:*module*:*function*:*name*
```

Some probes don't have a module or function identifier when they're listed. When providing the complete probe description for these probes, you must still include the empty fields:

```
dtrace:::BEGIN
```

Probes aren't required to have a module and function. The dtrace `BEGIN`, `END` and `ERROR` probes are good examples of this because these probes don't correspond to any specific instrumented program function or location. Instead, these probes are used for more abstract concepts, such as the idea of the end a tracing request. Other probes, such as those made available by the [Profile Provider](../reference/dtrace_providers_profile.md) or the [CPC Provider](../reference/dtrace_providers_cpc.md), also don't include module or function identifiers in their descriptions.

## D Programs <a id="concept_terms_programs">

You can bind a set of processing instructions called statements to one or more DTrace probes, so that when a probe fires, the specified statements are run to perform some required functionality. The set of enabled probes, the statements, and any conditions that might be evaluated when the probe fires, can all be collated into a *D program*.

A program can consist of several probe descriptions that decide which probes can trigger some functionality within the D program. Probe descriptions are followed by a set of processing instructions, called a *clause*, that describes what to do when the selected probe fires. Conditional expressions, called *predicates*, can be inserted between the probe descriptions and the clause to control the conditions under which the actions within the clause are run. For example, a program might be designed to fire for all system calls and to count these for a particular application. The program would consist of a probe description for the `syscall:::entry` probe, a predicate to limit processing to match either a process ID or the name of an executable, and a clause that performed the `count()` function to gather information about each system call function. The resulting D program might be:

```
*/\* Probe descriptions \*/*
syscall:::entry
*/\* Predicate \*/*
/execname=='date'/
*/\* Clause \*/*
{
@reads[probefunc]=count();
}
```

When the script is run it shows each system call that's made by the `date` command and provides the count value for each, as follows:

```
dtrace: description 'syscall:::entry ' matched 344 probes
Wed 22 Feb 11:54:51 GMT 2023

  exit_group                                                        1
  lseek                                                             1
  mmap                                                              1
  write                                                             1
  openat                                                            2
  read                                                              2
  brk                                                               3
  close                                                             4
  newfstat                                                          4
```

The program probe description matches all system call functions at the entry point. The program predicate evaluates a *built-in variable*, `execname`, against a string using an operator. The clause includes an *aggregation*, `@reads`, that's used to gather data about the firing probe. In this case, the aggregation stores a counter that increments every time the probe fires and the predicate resolves. The counter is implemented by the `count()` *function* and stores count values for each system call probe function. See [D Program Syntax Reference](../reference/d_program_syntax_reference.md) for more information on program structure and syntax.

## Aggregations <a id="concept_terms_aggregations">

*Aggregations* can be used to reduce large bodies of data to smaller, meaningful statistical metrics. Many common functions that are used to understand a set of data are aggregating functions. These functions include the following:

-   Counting the number of elements in the set.

-   Computing the minimum value of the set.

-   Computing the maximum value of the set.

-   Summing all the elements in the set.

-   Creating a histogram of the values in the set, as quantized into certain bins.


Although you could code an application to calculate an aggregation for a set of data, when many probes are firing concurrently, they can overwrite each other's updates to the aggregating variable or the calculation can become a serial bottleneck.

DTrace aggregation functions apply to the data as it's traced, so that the dataset doesn't need to be stored and the aggregation is always available as events occur. In this way, aggregation functions are more efficient and exact, and avoid overwrites. See [Aggregations](../reference/aggregation.md) for more information.

## Speculation <a id="concept_terms_speculation">

While predicates can be used to filter out uninteresting events, they're only useful if you already know which events you need to filter. Because DTrace is often used to help debug particular system behaviors, DTrace includes a set of *speculation* functions that can be used to trace data speculatively.

Speculation is used to trace quantities temporarily until particular information is known, at which case the data can be discarded or committed. By performing speculative tracing you can trace data until you know whether it's useful. For example, to trace data about events that might trigger a particular return code or error, you could speculatively trace all events and discard the trace data if it doesn't match the return code that you're interested in. See [Speculation](../reference/dtrace-ref-speculation.md) for more information.


