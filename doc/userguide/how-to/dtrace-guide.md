
# DTrace User Guide

This Linux DTrace User Guide describes how to use DTrace, which is a powerful dynamic tracing tool based on eBPF.
Most of the information in this document is generic and applies to all flavors of Linux on different Linux distributions.

## Get Started With DTrace

The topics in this section provide guidance on how to perform particular operations with DTrace and serve as an introduction to installing and using DTrace. By following steps in this guide, you can get started with DTrace immediately. After you have explored these topics, you can either review [DTrace Concepts](../explanation/dtrace-concepts.md) to get a better understanding of how DTrace works and how you can improve the way that you use it, or you can use the various references that are included to find out more about writing D programs that do what you need them to do.

-   **[Install DTrace](../how-to/dtrace-howto-install-dtrace.md#)**

-   **[List and Enable Probes](../how-to/dtrace-howto-list-and-enable-probes.md#)**
DTrace providers publish available probes to DTrace so that you can enable them to perform functions when they fire. You can use the `dtrace` command to list all available probes or to enable a probe.
-   **[Create a DTrace Script](../how-to/dtrace-howto-create-a-dtrace-script.md#)**
Learn how to create a DTrace script to develop understanding of the D Programming language.
-   **[Use Predicates For Control Flow](../how-to/dtrace-howto-use-predicates.md#)**
For runtime safety, one major difference between D and other programming languages such as C, C++, and the Java programming language is the absence of control-flow constructs such as `if`-statements and loops. D program clauses are written as single straight-line statement lists that trace an optional, fixed amount of data. D does provide the ability to conditionally trace data and change control flow using logical expressions called *predicates*. This tutorial shows how to use predicates to control D programs.

