# DTrace LLM Context Files

## Overview
These files provide structured **context packs** (`llms-txt` format) for use with large language models (LLMs) such as GPT-4 or Claude.
They teach the model how to write **correct, safe, and complete DTrace programs** for Oracle Linux.

The goal is to let engineers and administrators generate working D scripts in natural language — without having to memorize the entire D language syntax.

> The context files describe probe syntax, predicates, statements, variables, built-ins, functions, and safe tracing idioms for core providers (`syscall`, `proc`, `sched`, `profile`, `io`, `pid`, `usdt`, etc.).

---

## Files
| File | Description |
|------|--------------|
| `llms-dtrace-short.txt` | [Compact reference](llms-dtrace-short.txt) used to bootstrap LLMs for DTrace knowledge. |
| `llms-dtrace-complete.txt` | [Full expanded reference](llms-dtrace-complete.txt). |

---

## How to Use

1. **Install DTrace**

   ```bash
   sudo dnf install dtrace

   ```

   Make sure you are using the DTrace binary from the `dtrace-utils` package, _not_ the SystemTap compatibility binary:

   ```bash
   which dtrace
   sudo /usr/sbin/dtrace -V

   ```

2.  **Load the context file into your LLM session**

    -   In ChatGPT, Claude, or another interface that supports file context, click the “+” icon and upload `llms-dtrace-short.txt`.

    -   The model will automatically ingest the reference and understand how to write runnable DTrace programs for Oracle Linux.

3.  **Start asking questions in natural language**

    You can now prompt the model to produce valid D scripts:

    ```
    write a dtrace script that shows all read() syscalls made while the script is running

    ```

    or

    ```
    count how many processes start on the system within 5 seconds

    ```

4.  **Run the generated script**

    Save the model’s output (e.g. `trace.d`), then execute:

    ```bash
    sudo dtrace -s trace.d

    ```


----------

## Example Prompts

Here are a few good starting points:

>“Write a DTrace script that prints the PID of every process that accesses memory.”

Teaches process-level tracing and probe filters.

>“Provide a DTrace script to count how many processes are started over 5 seconds.”

Demonstrates use of timed aggregations.

>“Show all `read()` syscalls and count how many occur per process.”

Basic syscall entry/return tracing.

>“Track which executables call `open()` and print errors.”

Uses speculation and error guards.

----------

## Safety and Trust

DTrace runs in **nondestructive mode by default**.
Even if an LLM suggests unsafe actions, the runtime will prevent writes, signals, or other destructive operations unless explicitly enabled with `-w`.

This makes LLM-generated scripts safe to test and explore on production or staging systems — they can observe but not modify the kernel state.

----------

## Notes and Troubleshooting

-   If you see:

    ```
    dtrace: could not enable tracing: Destructive actions not allowed

    ```

    either remove `system()` calls or rerun with `sudo dtrace -w -s script.d`.

-   If `dtrace` behaves unexpectedly, ensure you are not invoking a SystemTap binary:

    ```bash
    sudo /usr/sbin/dtrace

    ```


----------

## Additional Resources

-   [Oracle DTrace Utils on GitHub](https://github.com/oracle/dtrace-utils)

-   [Oracle Linux DTrace Training Modules](https://oracle-samples.github.io/oltrain/tracks/ol/dtrace/)

-   [Oracle Linux Blog: Natural Language System Tracing with Dtrace and LLMs](https://blogs.oracle.com/linux/post/dtrace-and-llms)
