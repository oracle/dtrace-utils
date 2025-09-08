
# About DTrace {#concept_about}

DTrace is a powerful tracing tool that's available on Linux. DTrace has low overhead and is safe to use on production systems to analyze what a system is doing in real time.

DTrace lets you examine the behavior of user programs and the OS, to understand how the system works, to track down performance problems, and to find the causes of aberrant behavior. DTrace can collect or print stack traces, function arguments, timestamps, and statistical aggregates by using probes that can be runtime events or source-code locations.

Unlike many tracing tools, DTrace is fully programmable. You can collect data for one event and store it for use when another event is triggered. You can select what information you want to gather and how to report it. DTrace programs have a familiar syntax that draws on the C programming language.

This implementation of DTrace uses existing Linux kernel tracing facilities, such as eBPF, which didn't exist when DTrace was first ported to Linux. The new implementation removes DTrace dependencies on specialized kernel patches, but retains syntax compatibility with earlier implementations of DTrace to deliver a mature tracing tool based on modern technology. Furthermore, this implementation also maintains functional compatibility with earlier implementations of DTrace, so that you can perform the same actions using either version of DTrace.

This implementation is a user space application and is available on:

-   Gentoo Linux

-   Unbreakable Enterprise Kernel 8 \(UEK 8\) and later kernels on Oracle Linux 10.

-   Unbreakable Enterprise Kernel Release 7 \(UEK R7\) and later kernels on Oracle Linux 9.

-   Unbreakable Enterprise Kernel Release 6 \(UEK R6\) and later kernels on Oracle Linux 8.


DTrace is also available on Unbreakable Enterprise Kernel Release 6 \(UEK R6\) and later kernels on Oracle Linux 7, and requires the `libdtrace-ctf` library to run. The functionality of the `libdtrace-ctf` library is integrated into the Oracle Linux GNU tool chain for later Oracle Linux releases. Oracle Linux 7 is in Extended Support. Migrate applications and data to Oracle Linux 8, Oracle Linux 9, or Oracle Linux 10, as soon as possible.

DTrace is developed as an open source project available under the Universal Permissive License \(UPL\), Version 1.0. You can access source code and more information at [https://github.com/oracle/dtrace-utils](https://github.com/oracle/dtrace-utils).

**Parent topic:**[DTrace Concepts](../explanation/dtrace-concepts.md)

